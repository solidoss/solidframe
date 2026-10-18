// solid/serialization/v4/binaryserializer.hpp
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
#include <string_view>
#include <typeinfo>
#include <variant>

#include "solid/serialization/v4/binarybase.hpp"

namespace solid::serialization::inline v4::binary {

// —— Serializer ——
// Binary serializer driven by reflection v2, wire compatible with
// serialization v3 (see binarybase.hpp for the encoding of each kind of
// value).
//
// Values are written straight into the caller's buffer while it has room;
// what does not fit is left as tasks resumed by the next run() calls. Pending
// tasks keep references to the serialized fields (and to strings' bytes), so
// the serialized objects must outlive the serialization - exactly as in v3.
// Fields are reached through their make(...) descriptors; makef (accessor
// based) descriptors are not supported.
//
// Context is handed back to solidReflectV2 and to Callable fields, TypeId is
// the wire type of polymorphic pointers' type ids (an integral, or a pair of
// integrals for (category, id)).
template <class Context, typename TypeId = uint8_t>
class Serializer : NonCopyable {
    using ThisT      = Serializer<Context, TypeId>;
    using QueueT     = detail::TaskQueue<ThisT, Context>;
    using TypeMapT   = reflection::v2::TypeMapBase;
    using KindE      = detail::KindE;
    using TypeEntryT = TypeMapT::TypeEntry;

    struct Visitor {
        ThisT& rser_;

        template <class Id, class Field>
        void operator()(Id /*_id*/, Field&& _rfield, Context& _rctx) const
        {
            rser_.field(_rfield, _rctx);
        }
    };

    struct CopyState {
        std::array<char, max_compacted_size> buf_;
        uint8_t                              offset_;
        uint8_t                              size_;
    };

    QueueT          queue_;
    char*           pbeg_ = nullptr;
    char*           pcrt_ = nullptr;
    char*           pend_ = nullptr;
    ErrorConditionT error_;
    TypeMapT const* ptype_map_ = nullptr;

public:
    using ContextT = Context;

    static constexpr bool is_const_reflector = true;

    Serializer() = default;

    explicit Serializer(TypeMapT const& _rtype_map)
        : ptype_map_(&_rtype_map)
    {
    }

    // Serializes _rvalue: a reflectable object, a (polymorphic) pointer, a
    // container, a scalar...
    template <class T>
    ThisT& add(T const& _rvalue, Context& _rctx)
    {
        addValue(_rvalue, no_limit, _rctx);
        return *this;
    }

    // Prepares the buffer, lets _fnc(*this, _rctx) add the values and runs.
    // Returns the number of bytes written, -1 on error.
    template <class F>
    ptrdiff_t run(char* _pbeg, const size_t _sz, F&& _fnc, Context& _rctx)
    {
        prepare(_pbeg, _sz);
        std::forward<F>(_fnc)(*this, _rctx);
        queue_.run(*this, _rctx);
        return complete();
    }

    // Continues the pending serialization into a new buffer.
    ptrdiff_t run(char* _pbeg, const size_t _sz, Context& _rctx)
    {
        prepare(_pbeg, _sz);
        queue_.run(*this, _rctx);
        return complete();
    }

    template <class F>
    std::ostream& run(std::ostream& _ros, F&& _fnc, Context& _rctx)
    {
        constexpr size_t buf_cap = 8 * 1024;
        char             buf[buf_cap];

        clear();
        for (ptrdiff_t len = run(buf, buf_cap, std::forward<F>(_fnc), _rctx); len > 0; len = run(buf, buf_cap, _rctx)) {
            _ros.write(buf, len);
        }
        return _ros;
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

    // TypeMap hook: walks a polymorphic pointer's object as its dynamic type.
    template <class T>
    void reflect_object(T const& _rvalue, Context& _rctx)
    {
        addValue(_rvalue, no_limit, _rctx);
    }

private:
    void prepare(char* _pbeg, const size_t _sz) noexcept
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

    size_t room() const noexcept
    {
        return static_cast<size_t>(pend_ - pcrt_);
    }

    size_t writeSome(const char* _pdata, const size_t _sz) noexcept
    {
        const size_t sz = std::min(_sz, room());
        if (sz != 0) {
            std::memcpy(pcrt_, _pdata, sz);
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
        } else if constexpr (FieldT::type_id == TypeIdE::IStream || FieldT::type_id == TypeIdE::IOStream) {
            addStream(_rfield.stream(), _rfield.size, detail::progress_of(_rfield), _rctx);
        } else if constexpr (FieldT::type_id == TypeIdE::OStream) {
            // output streams are only filled by the deserializer
        } else {
            static_assert(reflection::v2::detail::FieldAccessibleC<FieldT>,
                "binary v4 serialization needs direct access to the field: use make(...), not makef(...)");
            addValue(reflection::v2::detail::Access::ref(_rfield), detail::limit_of(_rfield), _rctx);
        }
    }

    template <class F>
    void addCallable(F&& _fnc, Context& _rctx)
    {
        // The callable is kept alive in a task until whatever it scheduled is
        // done: those tasks may reference data the callable owns.
        queue_.tryRun(
            [fnc = std::forward<F>(_fnc), called = false](ThisT& _rser, Context& _rctx) mutable {
                if (called) {
                    return ReturnE::Done;
                }
                called = true;
                Visitor visitor{_rser};
                fnc(visitor, _rctx);
                return _rser.isRunEmpty() ? ReturnE::Done : ReturnE::Wait;
            },
            *this, _rctx);
    }

    template <class Progress>
    void addStream(std::istream& _ris, const uint64_t _size, Progress&& _progress, Context& _rctx)
    {
        queue_.tryRun(
            [pis = &_ris, progress = std::forward<Progress>(_progress), total = uint64_t{0}, _size](ThisT& _rser, Context& _rctx) mutable {
                return _rser.storeStreamChunk(*pis, total, _size, progress, _rctx);
            },
            *this, _rctx);
    }

    // —— values ——
    template <class T>
    void addValue(T const& _rvalue, const size_t _limit, Context& _rctx)
    {
        if (error_) {
            return;
        }
        constexpr KindE kind = detail::kind<T>();
        // Composite values are walked right away, like v3 does: their own
        // reflection may read the context (e.g. mprpc's message header),
        // which must be seen as it is now. Their leaves schedule themselves.
        if (isRunEmpty() || kind == KindE::Reflectable || kind == KindE::Pointer || kind == KindE::Optional || kind == KindE::Variant) {
            store(_rvalue, _limit, _rctx);
        } else if constexpr (detail::kind<T>() == KindE::Bool || detail::kind<T>() == KindE::Integer || detail::kind<T>() == KindE::Enum) {
            // by value: scalars are often temporaries computed in solidReflectV2
            queue_.schedule([value = _rvalue](ThisT& _rser, Context& _rctx) {
                _rser.store(value, no_limit, _rctx);
                return ReturnE::Done;
            });
        } else {
            queue_.schedule([pvalue = &_rvalue, _limit](ThisT& _rser, Context& _rctx) {
                _rser.store(*pvalue, _limit, _rctx);
                return ReturnE::Done;
            });
        }
    }

    template <class T>
    void store(T const& _rvalue, const size_t _limit, Context& _rctx)
    {
        constexpr KindE kind = detail::kind<T>();

        if constexpr (kind == KindE::Bool) {
            storeInteger(static_cast<uint8_t>(_rvalue ? 0xFF : 0xAA));
        } else if constexpr (kind == KindE::Integer) {
            storeInteger(_rvalue);
        } else if constexpr (kind == KindE::Enum) {
            storeInteger(static_cast<std::underlying_type_t<T>>(_rvalue));
        } else if constexpr (kind == KindE::String) {
            const std::string_view sv{_rvalue};
            if (sv.size() > _limit) {
                baseError(error_limit_string);
                return;
            }
            storeCompacted(sv.size());
            storeBinary(sv.data(), sv.size());
        } else if constexpr (kind == KindE::IntegralRange) {
            using ElementT    = std::remove_cvref_t<std::ranges::range_value_t<T>>;
            const size_t size = std::ranges::size(_rvalue);
            if (size > _limit) {
                baseError(error_limit_string);
                return;
            }
            storeCompacted(size);
            if constexpr (std::endian::native == std::endian::little || sizeof(ElementT) == 1) {
                storeBinary(reinterpret_cast<const char*>(std::ranges::data(_rvalue)), size * sizeof(ElementT));
            } else {
                storeItems(_rvalue, _rctx);
            }
        } else if constexpr (kind == KindE::IntegralArray) {
            using ElementT = typename T::value_type;
            if (_rvalue.size() > _limit) {
                baseError(error_limit_string);
                return;
            }
            storeCompacted(_rvalue.size());
            if constexpr (std::endian::native == std::endian::little || sizeof(ElementT) == 1) {
                storeBinary(reinterpret_cast<const char*>(_rvalue.data()), _rvalue.size() * sizeof(ElementT));
            } else {
                storeItems(_rvalue, _rctx);
            }
        } else if constexpr (kind == KindE::Array) {
            // like v3: only the first min(N, max_size) items travel
            const size_t count = std::min(_rvalue.size(), _limit);
            storeCompacted(count);
            storeItems(_rvalue, _rctx, count);
        } else if constexpr (kind == KindE::Bitset) {
            storeCompacted(_rvalue.size());
            storeBits(_rvalue, _rvalue.size());
        } else if constexpr (kind == KindE::BitVector) {
            if (_rvalue.size() > _limit) {
                baseError(error_limit_container);
                return;
            }
            storeCompacted(_rvalue.size());
            storeBits(_rvalue, _rvalue.size());
        } else if constexpr (kind == KindE::Container || kind == KindE::KeyContainer) {
            const size_t size = _rvalue.size();
            if (size > _limit) {
                baseError(error_limit_container);
                return;
            }
            storeCompacted(size);
            storeItems(_rvalue, _rctx);
        } else if constexpr (kind == KindE::Pointer) {
            storePointer(_rvalue, _rctx);
        } else if constexpr (kind == KindE::Optional) {
            storeInteger(static_cast<uint8_t>(_rvalue.has_value() ? 0xFF : 0xAA));
            if (_rvalue.has_value()) {
                addValue(*_rvalue, no_limit, _rctx);
            }
        } else if constexpr (kind == KindE::Variant) {
            storeCompacted(_rvalue.index());
            std::visit([this, &_rctx](auto const& _ralt) { this->addValue(_ralt, no_limit, _rctx); }, _rvalue);
        } else {
            reflection::v2::reflect(_rvalue, Visitor{*this}, _rctx);
        }
    }

    // With a typemap: the type id of the pointed object (0 for nullptr)
    // followed by the object walked as its dynamic type - as v3 does.
    // Without: a presence byte followed by the object; polymorphic element
    // types need a typemap.
    template <class P>
    void storePointer(P const& _rptr, Context& _rctx)
    {
        if (ptype_map_ != nullptr) {
            TypeEntryT const* pentry = nullptr;
            if (_rptr) {
                pentry = ptype_map_->find(typeid(*_rptr));
                if (pentry == nullptr) {
                    baseError(error_unknown_type);
                    return;
                }
            }
            storeTypeId(pentry);
            if (pentry != nullptr) {
                ptype_map_->reflect(*this, *_rptr, _rctx, *pentry);
            }
        } else if constexpr (!std::is_polymorphic_v<typename P::element_type>) {
            storeInteger(static_cast<uint8_t>(_rptr ? 0xFF : 0xAA));
            if (_rptr) {
                addValue(*_rptr, no_limit, _rctx);
            }
        } else {
            // a polymorphic object needs the typemap to travel as its own type
            baseError(error_unknown_type);
        }
    }

    void storeTypeId(TypeEntryT const* _pentry)
    {
        const size_t category = _pentry != nullptr ? _pentry->category() : 0;
        const size_t id       = _pentry != nullptr ? _pentry->id() : 0;
        if constexpr (detail::is_std_pair_v<TypeId>) {
            storeInteger(static_cast<typename TypeId::first_type>(category));
            storeInteger(static_cast<typename TypeId::second_type>(id));
        } else {
            storeInteger(static_cast<TypeId>(id));
        }
    }

    // —— items ——
    template <class C>
    void storeItems(C const& _rcont, Context& _rctx, const size_t _count = no_limit)
    {
        auto   it   = std::ranges::begin(_rcont);
        size_t left = std::min(_count, static_cast<size_t>(std::ranges::size(_rcont)));
        if (!storeSomeItems(it, left, _rctx)) {
            queue_.schedule([it, left](ThisT& _rser, Context& _rctx) mutable {
                return _rser.storeSomeItems(it, left, _rctx) ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class It>
    bool storeSomeItems(It& _rit, size_t& _rleft, Context& _rctx)
    {
        using ValueT = std::remove_cvref_t<decltype(*_rit)>;
        for (; _rleft != 0 && !error_; ++_rit, --_rleft) {
            if (!isRunEmpty() || (pcrt_ == pend_ && !std::is_empty_v<ValueT>)) {
                return false;
            }
            store(*_rit, no_limit, _rctx);
        }
        return true;
    }

    // —— primitives ——
    template <std::integral T>
    void storeInteger(const T _value)
    {
        if (isRunEmpty() && room() >= sizeof(T)) {
            pcrt_ = binary::store(pcrt_, _value);
            return;
        }
        CopyState state;
        binary::store(state.buf_.data(), _value);
        state.size_ = sizeof(T);
        storeCopy(state);
    }

    void storeCompacted(const uint64_t _value)
    {
        if (isRunEmpty() && room() >= max_compacted_size) {
            pcrt_ = binary::store_compact(pcrt_, _value);
            return;
        }
        CopyState state;
        state.size_ = static_cast<uint8_t>(binary::store_compact(state.buf_.data(), _value) - state.buf_.data());
        storeCopy(state);
    }

    // Small value encoded in _rstate: write what fits, keep the rest in a task.
    void storeCopy(CopyState& _rstate)
    {
        _rstate.offset_ = 0;
        if (isRunEmpty()) {
            _rstate.offset_ = static_cast<uint8_t>(writeSome(_rstate.buf_.data(), _rstate.size_));
            if (_rstate.offset_ == _rstate.size_) {
                return;
            }
        }
        queue_.schedule([state = _rstate](ThisT& _rser, Context&) mutable {
            state.offset_ += static_cast<uint8_t>(_rser.writeSome(state.buf_.data() + state.offset_, state.size_ - state.offset_));
            return state.offset_ == state.size_ ? ReturnE::Done : ReturnE::Wait;
        });
    }

    // Bytes owned by the serialized value: referenced, not copied, when they
    // do not fit.
    void storeBinary(const char* _pdata, size_t _sz)
    {
        if (isRunEmpty()) {
            const size_t sz = writeSome(_pdata, _sz);
            _pdata += sz;
            _sz -= sz;
        }
        if (_sz != 0) {
            queue_.schedule([_pdata, _sz](ThisT& _rser, Context&) mutable {
                const size_t sz = _rser.writeSome(_pdata, _sz);
                _pdata += sz;
                _sz -= sz;
                return _sz == 0 ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class C>
    void storeBits(C const& _rbits, const size_t _count)
    {
        size_t offset = 0;
        if (isRunEmpty()) {
            offset = writeBits(_rbits, offset, _count);
        }
        if (offset != _count) {
            queue_.schedule([pbits = &_rbits, offset, _count](ThisT& _rser, Context&) mutable {
                offset = _rser.writeBits(*pbits, offset, _count);
                return offset == _count ? ReturnE::Done : ReturnE::Wait;
            });
        }
    }

    template <class C>
    size_t writeBits(C const& _rbits, size_t _offset, const size_t _count) noexcept
    {
        while (_offset != _count && pcrt_ != pend_) {
            const size_t n    = std::min<size_t>(8, _count - _offset);
            unsigned     byte = 0;
            for (size_t i = 0; i < n; ++i) {
                byte |= static_cast<unsigned>(static_cast<bool>(_rbits[_offset + i])) << i;
            }
            *pcrt_++ = static_cast<char>(byte);
            _offset += n;
        }
        return _offset;
    }

    // One stream chunk: a 2 bytes length followed by that many bytes; a zero
    // length chunk ends the stream.
    // At most _size bytes are read from the stream (the descriptor's with_size).
    template <class Progress>
    ReturnE storeStreamChunk(std::istream& _ris, uint64_t& _rtotal, const uint64_t _size, Progress& _rprogress, Context& _rctx)
    {
        if (room() <= 2) {
            return ReturnE::Wait;
        }
        const size_t to_read = static_cast<size_t>(std::min<uint64_t>({room() - 2, std::numeric_limits<uint16_t>::max(), _size - _rtotal}));
        size_t       len     = 0;
        if (_ris && to_read != 0) {
            _ris.read(pcrt_ + 2, static_cast<std::streamsize>(to_read));
            len = static_cast<size_t>(_ris.gcount());
        }
        binary::store(pcrt_, static_cast<uint16_t>(len));
        pcrt_ += 2 + len;
        _rtotal += len;
        detail::notify_progress(_rprogress, _ris, _rtotal, len == 0, _rctx);
        return len == 0 ? ReturnE::Done : ReturnE::Continue;
    }
};

} // namespace solid::serialization::inline v4::binary
