// solid/serialization/v4/binarydeserializer.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <istream>
#include <ostream>
#include <utility>
#include <variant>

#include "solid/serialization/v4/binarybase.hpp"

namespace solid::serialization::v4::binary {

// —— Deserializer ——
// Counterpart of Serializer: fills objects from a byte stream received in
// buffers of any size (down to one byte at a time).
//
// While the buffer holds enough bytes values are decoded in place, with no
// allocation; a value split by the end of the buffer continues in a task on
// the next run() call. The fields being filled are referenced by those tasks,
// so the objects must outlive the deserialization.
template <class Context, typename TypeId = uint8_t>
class Deserializer : NonCopyable {
    using ThisT    = Deserializer<Context, TypeId>;
    using QueueT   = detail::TaskQueue<ThisT, Context>;
    using TypeMapT = reflection::v2::TypeMapBase;
    using KindE    = detail::KindE;

    struct Visitor {
        ThisT& rdes_;

        template <class Id, class Field>
        void operator()(Id /*_id*/, Field&& _rfield, Context& _rctx) const
        {
            rdes_.field(_rfield, _rctx);
        }
    };

    template <size_t Size>
    struct BytesState {
        std::array<char, Size> buf_;
        uint8_t                size_ = 0;
    };

    QueueT          queue_;
    const char*     pbeg_ = nullptr;
    const char*     pcrt_ = nullptr;
    const char*     pend_ = nullptr;
    ErrorConditionT error_;
    TypeMapT const* ptype_map_ = nullptr;

public:
    using ContextT = Context;

    static constexpr bool is_const_reflector = false;

    Deserializer() = default;

    explicit Deserializer(TypeMapT const& _rtype_map)
        : ptype_map_(&_rtype_map)
    {
    }

    template <class T>
    ThisT& add(T& _rvalue, Context& _rctx)
    {
        addValue(_rvalue, no_limit, _rctx);
        return *this;
    }

    // Prepares the buffer, lets _fnc(*this, _rctx) add the values and runs.
    // Returns the number of bytes consumed, -1 on error.
    template <class F>
    ptrdiff_t run(const char* _pbeg, const size_t _sz, F&& _fnc, Context& _rctx)
    {
        prepare(_pbeg, _sz);
        std::forward<F>(_fnc)(*this, _rctx);
        queue_.run(*this, _rctx);
        return complete();
    }

    // Continues the pending deserialization with a new buffer.
    ptrdiff_t run(const char* _pbeg, const size_t _sz, Context& _rctx)
    {
        prepare(_pbeg, _sz);
        queue_.run(*this, _rctx);
        return complete();
    }

    template <class F>
    std::istream& run(std::istream& _ris, F&& _fnc, Context& _rctx)
    {
        constexpr size_t buf_cap = 8 * 1024;
        char             buf[buf_cap];

        clear();
        _ris.read(buf, buf_cap);
        auto read_sz = static_cast<ptrdiff_t>(_ris.gcount());
        if (read_sz == 0) {
            return _ris;
        }
        ptrdiff_t rv = run(buf, static_cast<size_t>(read_sz), std::forward<F>(_fnc), _rctx);
        while (rv == read_sz && !empty()) {
            _ris.read(buf, buf_cap);
            read_sz = static_cast<ptrdiff_t>(_ris.gcount());
            if (read_sz == 0) {
                break;
            }
            rv = run(buf, static_cast<size_t>(read_sz), _rctx);
        }
        return _ris;
    }

    [[nodiscard]] ErrorConditionT const& error() const noexcept
    {
        return error_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return queue_.empty();
    }

    void clear()
    {
        queue_.clear();
        error_.clear();
    }

    // TypeMap hook: fills a just created polymorphic object as its own type.
    template <class T>
    void reflect_object(T& _rvalue, Context& _rctx)
    {
        addValue(_rvalue, no_limit, _rctx);
    }

private:
    void prepare(const char* _pbeg, const size_t _sz) noexcept
    {
        pbeg_ = pcrt_ = _pbeg;
        pend_         = _pbeg + _sz;
    }

    ptrdiff_t complete() noexcept
    {
        const ptrdiff_t rv = error_ ? -1 : pcrt_ - pbeg_;
        pbeg_ = pcrt_ = pend_ = nullptr;
        return rv;
    }

    void baseError(ErrorConditionT const& _rerr) noexcept
    {
        if (!error_) {
            error_ = _rerr;
            pend_  = pcrt_;
        }
    }

    bool isRunEmpty() const noexcept
    {
        return queue_.isRunEmpty();
    }

    size_t avail() const noexcept
    {
        return static_cast<size_t>(pend_ - pcrt_);
    }

    size_t readSome(char* _pdata, const size_t _sz) noexcept
    {
        const size_t sz = std::min(_sz, avail());
        if (sz != 0) {
            std::memcpy(_pdata, pcrt_, sz);
            pcrt_ += sz;
        }
        return sz;
    }

    // —— descriptors ——
    template <class Field>
    void field(Field& _rfield, Context& _rctx)
    {
        using FieldT = std::remove_cvref_t<Field>;
        using reflection::v2::TypeIdE;

        if (error_) {
            return;
        }
        if constexpr (FieldT::type_id == TypeIdE::Callable) {
            addCallable(detail::take_callable(_rfield), _rctx);
        } else if constexpr (FieldT::type_id == TypeIdE::OStream || FieldT::type_id == TypeIdE::IOStream) {
            addStream(_rfield.stream(), detail::progress_of(_rfield), _rctx);
        } else if constexpr (FieldT::type_id == TypeIdE::IStream) {
            // input streams are only read by the serializer
        } else {
            static_assert(reflection::v2::detail::FieldAccessibleC<FieldT>,
                "binary v4 deserialization needs direct access to the field: use make(...), not makef(...)");
            addValue(reflection::v2::detail::Access::ref(_rfield), detail::limit_of(_rfield), _rctx);
        }
    }

    template <class F>
    void addCallable(F&& _fnc, Context& _rctx)
    {
        // Deferred until the fields before it are filled in (it usually
        // decides based on them) and kept alive while what it scheduled runs.
        queue_.tryRun(
            [fnc = std::forward<F>(_fnc), called = false](ThisT& _rdes, Context& _rctx) mutable {
                if (called) {
                    return ReturnE::Done;
                }
                called = true;
                Visitor visitor{_rdes};
                fnc(visitor, _rctx);
                return _rdes.isRunEmpty() ? ReturnE::Done : ReturnE::Wait;
            },
            *this, _rctx);
    }

    template <class Progress>
    void addStream(std::ostream& _ros, Progress&& _progress, Context& _rctx)
    {
        queue_.tryRun(
            [pos = &_ros, progress = std::forward<Progress>(_progress), total = uint64_t{0}, left = size_t{0}, len = BytesState<2>{}, in_chunk = false](ThisT& _rdes, Context& _rctx) mutable {
                while (true) {
                    if (!in_chunk) {
                        len.size_ += static_cast<uint8_t>(_rdes.readSome(len.buf_.data() + len.size_, 2 - len.size_));
                        if (len.size_ != 2) {
                            return ReturnE::Wait;
                        }
                        len.size_ = 0;
                        uint16_t chunk_len;
                        binary::load(len.buf_.data(), chunk_len);
                        if (chunk_len == 0) {
                            detail::notify_progress(progress, *pos, total, true, _rctx);
                            return ReturnE::Done;
                        }
                        left     = chunk_len;
                        in_chunk = true;
                    }
                    const size_t sz = std::min(left, _rdes.avail());
                    if (sz == 0) {
                        return ReturnE::Wait;
                    }
                    pos->write(_rdes.pcrt_, static_cast<std::streamsize>(sz));
                    _rdes.pcrt_ += sz;
                    left -= sz;
                    total += sz;
                    if (left == 0) {
                        in_chunk = false;
                        detail::notify_progress(progress, *pos, total, false, _rctx);
                    }
                }
            },
            *this, _rctx);
    }

    // —— values ——
    template <class T>
    void addValue(T& _rvalue, const size_t _limit, Context& _rctx)
    {
        if (error_) {
            return;
        }
        if (isRunEmpty()) {
            load(_rvalue, _limit, _rctx);
        } else {
            queue_.schedule([pvalue = &_rvalue, _limit](ThisT& _rdes, Context& _rctx) {
                _rdes.load(*pvalue, _limit, _rctx);
                return ReturnE::Done;
            });
        }
    }

    template <class T>
    void load(T& _rvalue, const size_t _limit, Context& _rctx)
    {
        constexpr KindE kind = detail::kind<T>();

        if constexpr (kind == KindE::Bool) {
            loadInteger<uint8_t>([pvalue = &_rvalue](ThisT&, Context&, const uint8_t _v) { *pvalue = (_v == 0xFF); }, _rctx);
        } else if constexpr (kind == KindE::Integer) {
            loadInteger<T>([pvalue = &_rvalue](ThisT&, Context&, const T _v) { *pvalue = _v; }, _rctx);
        } else if constexpr (kind == KindE::Enum) {
            using UnderlyingT = std::underlying_type_t<T>;
            loadInteger<UnderlyingT>([pvalue = &_rvalue](ThisT&, Context&, const UnderlyingT _v) { *pvalue = static_cast<T>(_v); }, _rctx);
        } else if constexpr (kind == KindE::String) {
            static_assert(requires(T& _rs, const char* _p) { _rs.clear(); _rs.append(_p, size_t{}); _rs.assign(_p, size_t{}); }, "binary v4 deserialization needs a std::string like string");
            loadCompacted([pstr = &_rvalue, _limit](ThisT& _rdes, Context&, const uint64_t _sz) {
                if (_sz > _limit) {
                    _rdes.baseError(error_limit_string);
                    return;
                }
                _rdes.loadString(*pstr, static_cast<size_t>(_sz));
            },
                _rctx);
        } else if constexpr (kind == KindE::IntegralRange && requires(T& _rc) { _rc.resize(size_t{}); _rc.data(); }) {
            using ElementT = typename T::value_type;
            loadCompacted([pcont = &_rvalue, _limit](ThisT& _rdes, Context& _rctx, const uint64_t _sz) {
                if (_sz > _limit) {
                    _rdes.baseError(error_limit_string);
                    return;
                }
                if constexpr (std::endian::native == std::endian::little || sizeof(ElementT) == 1) {
                    pcont->resize(static_cast<size_t>(_sz));
                    _rdes.loadBinary(reinterpret_cast<char*>(pcont->data()), pcont->size() * sizeof(ElementT));
                } else {
                    pcont->clear();
                    _rdes.loadItems(*pcont, _sz, _rctx);
                }
            },
                _rctx);
        } else if constexpr (kind == KindE::IntegralArray) {
            using ElementT = typename T::value_type;
            loadCompacted([parr = &_rvalue, _limit](ThisT& _rdes, Context& _rctx, const uint64_t _sz) {
                if (_sz > _limit || _sz > parr->size()) {
                    _rdes.baseError(error_limit_string);
                    return;
                }
                if constexpr (std::endian::native == std::endian::little || sizeof(ElementT) == 1) {
                    _rdes.loadBinary(reinterpret_cast<char*>(parr->data()), static_cast<size_t>(_sz) * sizeof(ElementT));
                } else {
                    _rdes.loadArrayItems(*parr, static_cast<size_t>(_sz), _rctx);
                }
            },
                _rctx);
        } else if constexpr (kind == KindE::Array) {
            loadCompacted([parr = &_rvalue, _limit](ThisT& _rdes, Context& _rctx, const uint64_t _sz) {
                if (_sz > _limit || _sz > parr->size()) {
                    _rdes.baseError(error_limit_array);
                    return;
                }
                _rdes.loadArrayItems(*parr, static_cast<size_t>(_sz), _rctx);
            },
                _rctx);
        } else if constexpr (kind == KindE::Bitset) {
            loadCompacted([pbits = &_rvalue](ThisT& _rdes, Context&, const uint64_t _sz) {
                if (_sz > pbits->size()) {
                    _rdes.baseError(error_limit_container);
                    return;
                }
                pbits->reset();
                _rdes.loadBits(*pbits, static_cast<size_t>(_sz));
            },
                _rctx);
        } else if constexpr (kind == KindE::BitVector) {
            loadCompacted([pbits = &_rvalue, _limit](ThisT& _rdes, Context&, const uint64_t _sz) {
                if (_sz > _limit) {
                    _rdes.baseError(error_limit_container);
                    return;
                }
                pbits->clear();
                _rdes.loadBits(*pbits, static_cast<size_t>(_sz));
            },
                _rctx);
        } else if constexpr (kind == KindE::Container || kind == KindE::IntegralRange) {
            static_assert(requires(T& _rc) { _rc.emplace_back(); _rc.back(); _rc.clear(); }, "binary v4 deserialization needs emplace_back()/back()/clear() on sequence containers");
            loadCompacted([pcont = &_rvalue, _limit](ThisT& _rdes, Context& _rctx, const uint64_t _sz) {
                if (_sz > _limit) {
                    _rdes.baseError(error_limit_container);
                    return;
                }
                pcont->clear();
                if constexpr (requires { pcont->reserve(size_t{}); }) {
                    pcont->reserve(static_cast<size_t>(std::min<uint64_t>(_sz, 1024)));
                }
                _rdes.loadItems(*pcont, _sz, _rctx);
            },
                _rctx);
        } else if constexpr (kind == KindE::KeyContainer) {
            loadCompacted([pcont = &_rvalue, _limit](ThisT& _rdes, Context& _rctx, const uint64_t _sz) {
                if (_sz > _limit) {
                    _rdes.baseError(error_limit_container);
                    return;
                }
                pcont->clear();
                _rdes.loadKeyItems(*pcont, _sz, _rctx);
            },
                _rctx);
        } else if constexpr (kind == KindE::Pointer) {
            loadPointer(_rvalue, _rctx);
        } else if constexpr (kind == KindE::Optional) {
            loadInteger<uint8_t>([popt = &_rvalue](ThisT& _rdes, Context& _rctx, const uint8_t _v) {
                if (_v == 0xFF) {
                    popt->emplace();
                    _rdes.addValue(**popt, no_limit, _rctx);
                } else {
                    popt->reset();
                }
            },
                _rctx);
        } else if constexpr (kind == KindE::Variant) {
            loadCompacted([pvar = &_rvalue](ThisT& _rdes, Context& _rctx, const uint64_t _index) {
                if (_index >= std::variant_size_v<T>) {
                    _rdes.baseError(error_variant_index);
                    return;
                }
                _rdes.emplaceVariant(*pvar, static_cast<size_t>(_index), _rctx, std::make_index_sequence<std::variant_size_v<T>>{});
            },
                _rctx);
        } else {
            reflection::v2::reflect(_rvalue, Visitor{*this}, _rctx);
        }
    }

    template <class V, size_t... I>
    void emplaceVariant(V& _rvar, const size_t _index, Context& _rctx, std::index_sequence<I...>)
    {
        static_cast<void>(((I == _index ? (addValue(_rvar.template emplace<I>(), no_limit, _rctx), true) : false) || ...));
    }

    template <class P>
    void loadPointer(P& _rptr, Context& _rctx)
    {
        using ElementT = typename P::element_type;

        if (ptype_map_ != nullptr) {
            loadTypeId([pptr = &_rptr](ThisT& _rdes, Context& _rctx, const size_t _category, const size_t _id) {
                if (_id == 0) {
                    pptr->reset();
                    return;
                }
                const auto* pentry = _rdes.ptype_map_->find(reflection::v2::TypeCategory{_category}, reflection::v2::TypeId{_id});
                if (pentry == nullptr) {
                    pptr->reset();
                    _rdes.baseError(error_unknown_type);
                    return;
                }
                if (!_rdes.ptype_map_->create_and_reflect(_rdes, *pptr, _rctx, *pentry)) {
                    _rdes.baseError(error_reflector_type);
                }
            },
                _rctx);
        } else if constexpr (!std::is_polymorphic_v<ElementT> && std::is_default_constructible_v<ElementT>) {
            loadInteger<uint8_t>([pptr = &_rptr](ThisT& _rdes, Context& _rctx, const uint8_t _v) {
                if (_v == 0xFF) {
                    if constexpr (reflection::v2::detail::is_shared_ptr_v<P>) {
                        *pptr = std::make_shared<ElementT>();
                    } else {
                        *pptr = std::make_unique<ElementT>();
                    }
                    _rdes.addValue(**pptr, no_limit, _rctx);
                } else {
                    pptr->reset();
                }
            },
                _rctx);
        } else {
            // a polymorphic object needs the typemap to be created as its own type
            baseError(error_unknown_type);
        }
    }

    template <class F>
    void loadTypeId(F&& _then, Context& _rctx)
    {
        if constexpr (detail::is_std_pair_v<TypeId>) {
            using CategoryT = typename TypeId::first_type;
            using IdT       = typename TypeId::second_type;
            loadInteger<CategoryT>([then = std::forward<F>(_then)](ThisT& _rdes, Context& _rctx, const CategoryT _category) mutable {
                _rdes.template loadInteger<IdT>([then = std::move(then), _category](ThisT& _rdes, Context& _rctx, const IdT _id) mutable {
                    then(_rdes, _rctx, static_cast<size_t>(_category), static_cast<size_t>(_id));
                },
                    _rctx);
            },
                _rctx);
        } else {
            loadInteger<TypeId>([then = std::forward<F>(_then)](ThisT& _rdes, Context& _rctx, const TypeId _id) mutable {
                then(_rdes, _rctx, size_t{0}, static_cast<size_t>(_id));
            },
                _rctx);
        }
    }

    // —— items ——
    template <class C>
    void loadItems(C& _rcont, uint64_t _left, Context& _rctx)
    {
        if (!loadSomeItems(_rcont, _left, _rctx)) {
            queue_.schedule([pcont = &_rcont, _left](ThisT& _rdes, Context& _rctx) mutable {
                return _rdes.loadSomeItems(*pcont, _left, _rctx) ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    // Items are built in place (emplace_back): a vector may reallocate only
    // once the previous item is complete.
    template <class C>
    bool loadSomeItems(C& _rcont, uint64_t& _rleft, Context& _rctx)
    {
        using ElementT = typename C::value_type;
        for (; _rleft != 0 && !error_; --_rleft) {
            if (!isRunEmpty() || (pcrt_ == pend_ && !std::is_empty_v<ElementT>)) {
                return false;
            }
            _rcont.emplace_back();
            load(_rcont.back(), no_limit, _rctx);
        }
        return true;
    }

    template <class A>
    void loadArrayItems(A& _rarr, const size_t _count, Context& _rctx)
    {
        size_t index = 0;
        if (!loadSomeArrayItems(_rarr, index, _count, _rctx)) {
            queue_.schedule([parr = &_rarr, index, _count](ThisT& _rdes, Context& _rctx) mutable {
                return _rdes.loadSomeArrayItems(*parr, index, _count, _rctx) ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class A>
    bool loadSomeArrayItems(A& _rarr, size_t& _rindex, const size_t _count, Context& _rctx)
    {
        using ElementT = typename A::value_type;
        for (; _rindex != _count && !error_; ++_rindex) {
            if (!isRunEmpty() || (pcrt_ == pend_ && !std::is_empty_v<ElementT>)) {
                return false;
            }
            load(_rarr[_rindex], no_limit, _rctx);
        }
        return true;
    }

    // (Multi)sets and maps: each item is built in a local owned by the task,
    // then moved in.
    template <class C>
    void loadKeyItems(C& _rcont, const uint64_t _count, Context& _rctx)
    {
        using ItemT = typename detail::key_item<typename C::value_type>::type;
        queue_.tryRun(
            [pcont = &_rcont, left = _count, item = ItemT{}, pending = false](ThisT& _rdes, Context& _rctx) mutable {
                if (pending) {
                    pcont->emplace(std::move(item));
                    pending = false;
                }
                for (; left != 0 && !_rdes.error_; --left) {
                    if (!_rdes.isRunEmpty() || (_rdes.pcrt_ == _rdes.pend_ && !std::is_empty_v<ItemT>)) {
                        return ReturnE::Wait;
                    }
                    item = ItemT{};
                    _rdes.load(item, no_limit, _rctx);
                    if (!_rdes.isRunEmpty()) {
                        pending = true;
                        --left;
                        return ReturnE::Wait;
                    }
                    pcont->emplace(std::move(item));
                }
                return ReturnE::Done;
            },
            *this, _rctx);
    }

    // —— primitives ——
    // Each one decodes a value then hands it to _then(*this, ctx, value):
    // inline when the bytes are there, from a task otherwise.
    template <std::integral T, class F>
    void loadInteger(F&& _then, Context& _rctx)
    {
        if (isRunEmpty() && avail() >= sizeof(T)) {
            T value;
            pcrt_ = binary::load(pcrt_, value);
            _then(*this, _rctx, value);
            return;
        }
        BytesState<sizeof(T)> state;
        if (isRunEmpty()) {
            state.size_ = static_cast<uint8_t>(readSome(state.buf_.data(), sizeof(T)));
        }
        queue_.schedule([state, then = std::forward<F>(_then)](ThisT& _rdes, Context& _rctx) mutable {
            state.size_ += static_cast<uint8_t>(_rdes.readSome(state.buf_.data() + state.size_, sizeof(T) - state.size_));
            if (state.size_ != sizeof(T)) {
                return ReturnE::Wait;
            }
            T value;
            binary::load(state.buf_.data(), value);
            then(_rdes, _rctx, value);
            return ReturnE::Done;
        });
    }

    template <class F>
    void loadCompacted(F&& _then, Context& _rctx)
    {
        BytesState<max_compacted_size> state;
        if (isRunEmpty()) {
            uint64_t    value;
            const char* p = binary::load_compact(pcrt_, avail(), value);
            if (p == pcrt_) {
                baseError(error_compacted_integer);
                return;
            }
            if (p != nullptr) {
                pcrt_ = p;
                _then(*this, _rctx, value);
                return;
            }
            state.size_ = static_cast<uint8_t>(readSome(state.buf_.data(), avail()));
        }
        queue_.schedule([state, then = std::forward<F>(_then)](ThisT& _rdes, Context& _rctx) mutable {
            if (state.size_ == 0) {
                state.size_ = static_cast<uint8_t>(_rdes.readSome(state.buf_.data(), 1));
                if (state.size_ == 0) {
                    return ReturnE::Wait;
                }
            }
            const size_t size = binary::compact_size(state.buf_.data(), state.size_);
            if (size > max_compacted_size) {
                _rdes.baseError(error_compacted_integer);
                return ReturnE::Done;
            }
            state.size_ += static_cast<uint8_t>(_rdes.readSome(state.buf_.data() + state.size_, size - state.size_));
            if (state.size_ != size) {
                return ReturnE::Wait;
            }
            uint64_t value;
            binary::load_compact(state.buf_.data(), size, value);
            then(_rdes, _rctx, value);
            return ReturnE::Done;
        });
    }

    template <class S>
    void loadString(S& _rstr, size_t _sz)
    {
        if (isRunEmpty() && _sz <= avail()) {
            _rstr.assign(pcrt_, _sz);
            pcrt_ += _sz;
            return;
        }
        // the string spans buffers: allocate once, as v3's resize does - the
        // size was checked against the field's max_size
        _rstr.clear();
        _rstr.reserve(_sz);
        if (isRunEmpty()) {
            const size_t sz = avail();
            _rstr.append(pcrt_, sz);
            pcrt_ += sz;
            _sz -= sz;
        }
        queue_.schedule([pstr = &_rstr, _sz](ThisT& _rdes, Context&) mutable {
            const size_t sz = std::min(_sz, _rdes.avail());
            pstr->append(_rdes.pcrt_, sz);
            _rdes.pcrt_ += sz;
            _sz -= sz;
            return _sz == 0 ? ReturnE::Done : ReturnE::Wait;
        });
    }

    void loadBinary(char* _pdata, size_t _sz)
    {
        if (isRunEmpty()) {
            const size_t sz = readSome(_pdata, _sz);
            _pdata += sz;
            _sz -= sz;
        }
        if (_sz != 0) {
            queue_.schedule([_pdata, _sz](ThisT& _rdes, Context&) mutable {
                const size_t sz = _rdes.readSome(_pdata, _sz);
                _pdata += sz;
                _sz -= sz;
                return _sz == 0 ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class C>
    void loadBits(C& _rbits, const size_t _count)
    {
        size_t offset = 0;
        if (isRunEmpty()) {
            offset = readBits(_rbits, offset, _count);
        }
        if (offset != _count) {
            queue_.schedule([pbits = &_rbits, offset, _count](ThisT& _rdes, Context&) mutable {
                offset = _rdes.readBits(*pbits, offset, _count);
                return offset == _count ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class C>
    size_t readBits(C& _rbits, size_t _offset, const size_t _count) noexcept
    {
        while (_offset != _count && pcrt_ != pend_) {
            const auto   byte = static_cast<uint8_t>(*pcrt_++);
            const size_t n    = std::min<size_t>(8, _count - _offset);
            for (size_t i = 0; i < n; ++i) {
                const bool bit = ((byte >> i) & 1) != 0;
                if constexpr (reflection::v2::detail::BitsetC<C>) {
                    _rbits.set(_offset + i, bit);
                } else {
                    _rbits.push_back(bit);
                }
            }
            _offset += n;
        }
        return _offset;
    }
};

} // namespace solid::serialization::v4::binary
