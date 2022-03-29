
// solid/serialization/v3/binaryserializer.hpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <istream>
#include <list>
#include <ostream>
#include <string>

#include "solid/reflection/v1/dispatch.hpp"
#include "solid/serialization/v3/binarybase.hpp"
#include "solid/serialization/v3/binarybasic.hpp"
#include "solid/system/convertors.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/innerlist.hpp"
#include "solid/utility/ioformat.hpp"

namespace solid {
namespace serialization {
namespace v3 {
namespace binary {

class SerializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(SerializerBase&, Runnable&, void*);

    using FunctionT = solid_function_t(ReturnE(SerializerBase&, Runnable&, void*));

    struct Runnable {
        Runnable(
            const void* _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
        {
        }

        template <class F>
        Runnable(const void* _ptr, CallbackT _call, const char* _name, F&& _f)
            : ptr_(_ptr)
            , call_(_call)
            , size_(0)
            , data_(0)
            , name_(_name)
            , fnc_(std::move(_f))
        {
        }

        template <class F>
        Runnable(
            const void* _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            const char* _name,
            F&&         _f)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
            , fnc_(std::move(_f))
        {
        }

        void clear()
        {
            ptr_  = nullptr;
            call_ = nullptr;
            solid_function_clear(fnc_);
        }

        const void* ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
    };

    using RunListT         = std::list<Runnable>;
    using RunListIteratorT = std::list<Runnable>::const_iterator;

protected:
    SerializerBase(const reflection::v1::TypeMapBase* const _ptype_map);

public:
    static constexpr bool is_const_reflector = true;

    std::ostream& run(std::ostream& _ros, void* _pctx = nullptr);
    ptrdiff_t     run(char* _pbeg, unsigned _sz, void* _pctx = nullptr);

    void clear();

    bool empty() const
    {
        return run_lst_.empty();
    }

public: //should be protected
    inline void addBasic(const bool& _rb, const char* _name)
    {
        solid_log(logger, Info, _name);
        Runnable r{nullptr, &store_byte, 1, static_cast<uint64_t>(_rb ? 0xFF : 0xAA), _name};

        if (isRunEmpty()) {
            if (doStoreByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(const int8_t& _rb, const char* _name)
    {
        solid_log(logger, Info, _name);
        Runnable r{nullptr, &store_byte, 1, static_cast<uint64_t>(_rb), _name};

        if (isRunEmpty()) {
            if (doStoreByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(const uint8_t& _rb, const char* _name)
    {
        solid_log(logger, Info, _name);
        Runnable r{nullptr, &store_byte, 1, static_cast<uint64_t>(_rb), _name};

        if (isRunEmpty()) {
            if (doStoreByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(const std::string& _rb, const uint64_t _limit, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rb.size() << ' ' << _limit << ' ' << trim_str(_rb.c_str(), _rb.size(), 4, 4));

        if (_rb.size() > _limit) {
            baseError(error_limit_string);
            return;
        }

        addBasicWithCheck(_rb.size(), _name);

        Runnable r{_rb.data(), &store_binary, _rb.size(), 0, _name};

        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <typename T, class A>
    inline void addVectorChar(const std::vector<T, A>& _rb, const uint64_t _limit, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rb.size() << ' ' << _limit);

        if (_rb.size() > _limit) {
            baseError(error_limit_string);
            return;
        }

        addBasicWithCheck(_rb.size(), _name);

        Runnable r{_rb.data(), &store_binary, _rb.size(), 0, _name};

        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class A>
    inline void addVectorChar(const std::vector<uint8_t, A>& _rb, const uint64_t _limit, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rb.size() << ' ' << _limit);

        if (_rb.size() > _limit) {
            baseError(error_limit_string);
            return;
        }

        addBasicWithCheck(_rb.size(), _name);

        Runnable r{_rb.data(), &store_binary, _rb.size(), 0, _name};

        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <typename T>
    void addBasic(const T& _rb, const char* _name)
    {
        static_assert(std::is_integral_v<T>, "only integral types are accepted");
        solid_log(logger, Info, _name << ' ' << _rb);
        Runnable r{std::addressof(_rb), &store_cross, sizeof(T), static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreCross(r) == ReturnE::Done) {
                return;
            }
        }
        schedule(std::move(r));
    }

    template <typename T>
    inline void addBasicWithCheck(const T& _rb, const char* _name)
    {
        static_assert(std::is_integral_v<T>, "only integral types are accepted");
        solid_log(logger, Info, _name << ' ' << _rb);
        Runnable r{nullptr, &store_cross_with_check, 0, static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreCrossWithCheck(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class S, class F, class Ctx>
    void addFunction(S& _rs, F&& _f, Ctx& _rctx, const char* _name)
    {
        solid_log(logger, Info, _name);
        if (isRunEmpty() && _rs.pcrt_ != _rs.pend_) {
            _f(_rs, _rctx);
        } else {
            Runnable r{
                nullptr,
                call_function,
                _name,
                [_f = std::move(_f)](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
                    const RunListIteratorT old_sentinel = _rs.sentinel();

                    _f(static_cast<S&>(_rs), *static_cast<Ctx*>(_pctx));

                    const bool is_run_empty = _rs.isRunEmpty();
                    _rs.sentinel(old_sentinel);
                    if (is_run_empty) {
                        return Base::ReturnE::Done;
                    } else {
                        _rr.call_ = noop;
                        return Base::ReturnE::Wait;
                    }
                }};
            schedule(std::move(r));
        }
    }

    template <class S, class F>
    void pushFunction(S& _rs, F _f, const char* _name)
    {
        solid_log(logger, Info, _name);
        auto lambda = [_f = std::move(_f)](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
            const RunListIteratorT old_sentinel = _rs.sentinel();
            const bool             done         = _f(static_cast<S&>(_rs), _rr.name_);

            const bool is_run_empty = _rs.isRunEmpty();
            _rs.sentinel(old_sentinel);

            if (done) {
                _rr.call_ = noop;
            }
            if (is_run_empty) {
                if (done) {
                    return Base::ReturnE::Done;
                } else {
                    return Base::ReturnE::Continue;
                }
            } else {
                if (done) {
                    _rr.call_ = noop;
                }
                return Base::ReturnE::Wait;
            }
        };
        Runnable r{nullptr, call_function, std::move(lambda), _name};

        tryRun(std::move(r));
    }

    template <class S, class F, class Ctx>
    void pushFunction(S& _rs, F _f, Ctx& _rctx, const char* _name)
    {
        solid_log(logger, Info, _name);
        auto lambda = [_f = std::move(_f)](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
            const RunListIteratorT old_sentinel = _rs.sentinel();
            const bool             done         = _f(static_cast<S&>(_rs), *static_cast<Ctx*>(_pctx), _rr.name_);

            const bool is_run_empty = _rs.isRunEmpty();
            _rs.sentinel(old_sentinel);

            if (done) {
                _rr.call_ = noop;
            }
            if (is_run_empty) {
                if (done) {
                    return Base::ReturnE::Done;
                } else {
                    return Base::ReturnE::Continue;
                }
            } else {
                if (done) {
                    _rr.call_ = noop;
                }
                return Base::ReturnE::Wait;
            }
        };
        Runnable r{nullptr, call_function, std::move(lambda), _name};

        tryRun(std::move(r), &_rctx);
    }

    template <class S, class C, class Ctx>
    void addContainer(S& _rs, const C& _rc, const uint64_t _limit, Ctx& _rctx, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rc.size() << ' ' << _limit);

        if (_rc.size() > _limit) {
            baseError(error_limit_container);
            return;
        }

        addBasicWithCheck(_rc.size(), _name);

        if (_rc.size()) {
            typename C::const_iterator it = _rc.cbegin();

            while (_rs.pcrt_ != _rs.pend_ && it != _rc.cend()) {
                _rs.add(*it, _rctx, 0, _name); //TODO: use index instead of 0
                ++it;
            }

            if (it != _rc.cend()) {
                auto lambda = [it](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
                    const C&               rcontainer   = *static_cast<const C*>(_rr.ptr_);
                    Ctx&                   rctx         = *static_cast<Ctx*>(_pctx);
                    S&                     rs           = static_cast<S&>(_rs);
                    const RunListIteratorT old_sentinel = _rs.sentinel();

                    while (_rs.pcrt_ != _rs.pend_ && it != rcontainer.cend()) {
                        rs.add(*it, rctx, 0, _rr.name_); //TODO: use index instead of 0
                        ++it;
                    }

                    const bool is_run_empty = _rs.isRunEmpty();
                    _rs.sentinel(old_sentinel);

                    if (it != rcontainer.cend() || !is_run_empty) {
                        return ReturnE::Wait;
                    } else {
                        return ReturnE::Done;
                    }
                };

                Runnable r{&_rc, call_function, _name, lambda};
                schedule(std::move(r));
            }
        }
    }

    template <class F, class Ctx>
    void addStream(std::istream& _ris, const uint64_t _sz, const uint64_t _limit, F&& _f, Ctx& _rctx, const size_t _index, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _sz << ' ' << _limit);

        auto lambda = [_f = std::move(_f), _limit, _index](SerializerBase& _rs, Runnable& _rr, void* _pctx) {
            std::istream& ris  = *const_cast<std::istream*>(static_cast<const std::istream*>(_rr.ptr_));
            Ctx&          rctx = *static_cast<Ctx*>(_pctx);
            if (_rr.data_ > _limit) {
                _rs.baseError(error_limit_stream);
                return ReturnE::Done;
            }
            _f(rctx, ris, _rr.data_, _rr.size_ == 0, _index, _rr.name_);
            return ReturnE::Done;
        };

        Runnable r{&_ris, &store_stream, _sz, 0, _name, lambda};

        if (isRunEmpty()) {
            if (store_stream(*this, r, &_rctx) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    void addBinary(const void* _pv, const size_t _sz, const char* _name)
    {
        Runnable r{_pv, &store_binary, _sz, 0, _name};

        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    void addBlob(const void* _pv, const size_t _sz, const size_t _cp, const char* _name)
    {
        if (_sz > _cp) {
            baseError(error_limit_blob);
            return;
        }

        addBasicWithCheck(_sz, _name);

        Runnable r{_pv, &store_binary, _sz, 0, _name};

        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <typename A>
    void addVectorBool(const std::vector<bool, A>& _rc, const uint64_t _limit, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rc.size() << ' ' << _limit);

        if (_rc.size() > _limit) {
            baseError(error_limit_container);
            return;
        }

        addBasicWithCheck(_rc.size(), _name);

        Runnable r{&_rc, &store_vector_bool<A>, _rc.size(), 0, _name};

        if (isRunEmpty()) {
            if (store_vector_bool<A>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <size_t N>
    void addBitset(const std::bitset<N>& _rc, const char* _name)
    {
        solid_log(logger, Info, _name << ' ' << _rc.size());

        addBasicWithCheck(N, _name);

        Runnable r{&_rc, &store_bitset<N>, N, 0, _name};

        if (isRunEmpty()) {
            if (store_bitset<N>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class S, class T, size_t N, class C>
    void addArray(S& _rs, const std::array<T, N>& _rc, const size_t _sz, C& _rctx, const uint64_t _limit, const char* _name)
    {
        if (_limit != 0 && _sz > _limit) {
            baseError(error_limit_container);
            return;
        }

        addBasicWithCheck(_sz, _name);

        Runnable r{&_rc, &store_array<S, T, N, C>, _sz, 0, _name};

        tryRun(std::move(r));
    }

protected:
    void doPrepareRun(char* _pbeg, unsigned _sz)
    {
        pbeg_ = _pbeg;
        pend_ = _pbeg + _sz;
        pcrt_ = _pbeg;
    }
    ptrdiff_t doRun(void* _pctx = nullptr);
    void      baseError(const ErrorConditionT& _err)
    {
        if (!error_) {
            error_ = _err;
            pcrt_ = pbeg_ = pend_ = nullptr;
        }
    }

private:
    void tryRun(Runnable&& _ur, void* _pctx = nullptr);

    RunListIteratorT sentinel()
    {
        RunListIteratorT old = sentinel_;
        sentinel_            = run_lst_.cbegin();
        return old;
    }

    void sentinel(const RunListIteratorT _s)
    {
        sentinel_ = _s;
    }

    bool isRunEmpty() const
    {
        return sentinel_ == run_lst_.cbegin();
    }

    RunListIteratorT schedule(Runnable&& _ur)
    {
        return run_lst_.emplace(sentinel_, std::move(_ur));
    }

    static ReturnE store_byte(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_cross(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_cross_with_check(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_binary(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE call_function(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE noop(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE store_stream(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    template <typename A>
    static ReturnE store_vector_bool(SerializerBase& _rs, Runnable& _rr, void* _pctx)
    {
        if (_rs.pcrt_ != _rs.pend_) {
            size_t towrite = (_rs.pend_ - _rs.pcrt_) << 3; //*8 bits

            if (towrite > _rr.size_) {
                towrite = static_cast<size_t>(_rr.size_);
            }

            const std::vector<bool, A>& vec = *reinterpret_cast<const std::vector<bool, A>*>(_rr.ptr_);

            for (size_t i = 0; i < towrite; ++i) {
                store_bit_at(reinterpret_cast<uint8_t*>(_rs.pcrt_), i, vec[static_cast<size_t>(_rr.data_ + i)]);
            }

            _rs.pcrt_ += (towrite >> 3);

            if ((towrite & 7) != 0) {
                ++_rs.pcrt_;
            }
            _rr.size_ -= towrite;
            _rr.data_ += towrite;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }

    template <size_t N>
    static ReturnE store_bitset(SerializerBase& _rs, Runnable& _rr, void* _pctx)
    {
        if (_rs.pcrt_ != _rs.pend_) {
            size_t towrite = (_rs.pend_ - _rs.pcrt_) << 3; //*8 bits

            if (towrite > _rr.size_) {
                towrite = static_cast<size_t>(_rr.size_);
            }

            const std::bitset<N>& bs = *reinterpret_cast<const std::bitset<N>*>(_rr.ptr_);

            if ((towrite & 7) != 0) {
                *(_rs.pcrt_ + (towrite >> 3)) = 0; //reset the last byte entirely - valgrind complains about it
            }

            for (size_t i = 0; i < towrite; ++i) {
                store_bit_at(reinterpret_cast<uint8_t*>(_rs.pcrt_), i, bs[static_cast<size_t>(_rr.data_ + i)]);
            }

            _rs.pcrt_ += (towrite >> 3);

            if ((towrite & 7) != 0) {
                ++_rs.pcrt_;
            }
            _rr.size_ -= towrite;
            _rr.data_ += towrite;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }
    template <class S, class T, size_t N, class Ctx>
    static ReturnE store_array(SerializerBase& _rs, Runnable& _rr, void* _pctx)
    {
        const std::array<T, N>& rcontainer   = *static_cast<const std::array<T, N>*>(_rr.ptr_);
        Ctx&                    rctx         = *static_cast<Ctx*>(_pctx);
        S&                      rs           = static_cast<S&>(_rs);
        const RunListIteratorT  old_sentinel = _rs.sentinel();

        while (_rs.pcrt_ != _rs.pend_ && _rr.data_ < _rr.size_) {
            rs.add(rcontainer[static_cast<size_t>(_rr.data_)], rctx, _rr.data_, _rr.name_);
            ++_rr.data_;
        }

        const bool is_run_empty = _rs.isRunEmpty();
        _rs.sentinel(old_sentinel);

        if (_rr.data_ != _rr.size_ || !is_run_empty) {
            return ReturnE::Wait;
        } else {
            return ReturnE::Done;
        }
    }

private:
    inline Base::ReturnE doStoreByte(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            const char c = static_cast<char>(_rr.data_);
            *pcrt_       = c;
            ++pcrt_;
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    }

    inline Base::ReturnE doStoreBinary(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            size_t towrite = pend_ - pcrt_;

            if (towrite > _rr.size_) {
                towrite = static_cast<size_t>(_rr.size_);
            }

            memcpy(pcrt_, _rr.ptr_, towrite);

            pcrt_ += towrite;
            _rr.size_ -= towrite;

            _rr.ptr_ = reinterpret_cast<const uint8_t*>(_rr.ptr_) + towrite;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }

    inline Base::ReturnE doStoreCrossWithCheck(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            char* p = cross::store_with_check(pcrt_, pend_ - pcrt_, _rr.data_);
            if (p != nullptr) {
                pcrt_ = p;
                return ReturnE::Done;
            } else {
                p = cross::store_with_check(data_.buf_, BufferCapacityE, _rr.data_);
                solid_check_log(p, logger, "should not be null");
                _rr.ptr_  = data_.buf_;
                _rr.size_ = p - data_.buf_;
                _rr.data_ = 0;
                _rr.call_ = store_binary;
                return doStoreBinary(_rr);
            }
        }
        return ReturnE::Wait;
    }

    inline Base::ReturnE doStoreCross(Runnable& _rr)
    {
#if 1
        if (pcrt_ != pend_) {
            const size_t sz = max_padded_byte_cout(_rr.data_);
            *pcrt_          = static_cast<char>(sz);
            solid_log(logger, Info, "sz = " << sz << " c = " << (int)*pcrt_ << " data = " << _rr.data_)++ pcrt_;
            _rr.size_ = sz;
            _rr.call_ = store_binary;
#ifdef SOLID_ON_BIG_ENDIAN
            data_.u64_ = swap_bytes(_rr.data_);
#else
            data_.u64_ = _rr.data_;
#endif
            _rr.ptr_ = data_.buf_;
            return doStoreBinary(_rr);
        }
        return ReturnE::Wait;
#else
        if (pcrt_ != pend_) {
            _rr.call_ = store_binary;
            _rr.ptr_  = &_rr.data_;
            return doStoreBinary(_rr);
        }
        return ReturnE::Wait;
#endif
    }

protected:
    enum {
        BufferCapacityE = sizeof(uint64_t) * 1
    };
    union {
        char     buf_[BufferCapacityE];
        uint64_t u64_;
        void*    p_;
    } data_;
    const reflection::v1::TypeMapBase* const ptype_map_;

private:
    char*            pbeg_;
    char*            pend_;
    char*            pcrt_;
    RunListT         run_lst_;
    RunListIteratorT sentinel_;
}; // namespace v2

//-----------------------------------------------------------------------------

template <class MetadataVariant, class MetadataFactory, class Context, typename TypeId>
class Serializer : public SerializerBase {
    const MetadataFactory& rmetadata_factory_;
    TypeId                 type_id_;

public:
    using ContextT = Context;
    using ThisT    = Serializer<MetadataVariant, MetadataFactory, Context, Context>;

    Serializer(
        MetadataFactory& _rmetadata_factory, const reflection::v1::TypeMapBase& _rtype_map)
        : SerializerBase(&_rtype_map)
        , rmetadata_factory_(_rmetadata_factory)
    {
    }

    Serializer(
        MetadataFactory& _rmetadata_factory)
        : SerializerBase(nullptr)
        , rmetadata_factory_(_rmetadata_factory)
    {
    }

    template <typename T, typename F>
    auto& add(const T& _rt, Context& _rctx, const size_t _id, const char* const _name, F _f)
    {
        auto meta = rmetadata_factory_(_rt, _rctx, this->ptype_map_);
        _f(meta);

        addDispatch(meta, _rt, _rctx, _id, _name);
        return *this;
    }

    template <typename T>
    auto& add(const T& _rt, Context& _rctx, const size_t _id, const char* const _name)
    {
        auto meta = rmetadata_factory_(_rt, _rctx, this->ptype_map_);
        addDispatch(meta, _rt, _rctx, _id, _name);
        return *this;
    }

    template <typename T>
    auto& add(T&& _rt, Context& _rctx)
    {
        //static_assert(std::is_invocable_v<T, ThisT &, Context&>, "Parameter should be invocable");
        //std::invoke(_rt, *this, _rctx);
        this->addFunction(*this, std::forward<T>(_rt), _rctx, "function");
        return *this;
    }
#if 0
    template <typename F>
    ThisT& add(std::istream& _ris, const uint64_t _sz, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ris, _sz, limits().stream(), _f, _rctx, _name);
        return *this;
    }

    template <typename F>
    ThisT& add(std::istream& _ris, const uint64_t _sz, const Limit _limit, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ris, _sz, _limit.value_, _f, _rctx, _name);
        return *this;
    }

    template <typename F>
    ThisT& add(std::istream& _ris, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ris, InvalidSize(), limits().stream(), _f, _rctx, _name);
        return *this;
    }

    template <typename F>
    ThisT& add(std::istream& _ris, const Limit _limit, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ris, InvalidSize(), _limit.value_, _f, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(const T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }

    template <typename T, size_t N>
    ThisT& add(const std::array<T, N>& _rt, const size_t _sz, Ctx& _rctx, const char* _name)
    {
        addArray(*this, _rt, _sz, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(T& _rt, const Limit _limit, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _limit.value_, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(const T& _rt, const Limit _limit, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _limit.value_, _rctx, _name);
        return *this;
    }

    ThisT& add(const void* _pv, const size_t _sz, const size_t _cp, Ctx& /*_rctx*/, const char* _name)
    {
        addBlob(_pv, _sz, _cp, _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T&& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _rctx, _name);
        return *this;
    }
    
    template <class T>
    void addPointer(const std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        solid_log(logger, Info, _name);
        const T*        p = _rp.get();
        ErrorConditionT err;
        const size_t    idx = rtype_map_.id(type_id_, p, err);

        if (!err) {
            add(type_id_, _rctx, _name);
            rtype_map_.serialize(*this, p, idx, _rctx, _name);
        } else {
            SerializerBase::baseError(err);
        }
    }

    template <class T, class D>
    void addPointer(const std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        solid_log(logger, Info, _name);
        const T*        p = _rp.get();
        ErrorConditionT err;
        const size_t    idx = rtype_map_.id(type_id_, p, err);

        if (!err) {
            add(type_id_, _rctx, _name);
            rtype_map_.serialize(*this, p, idx, _rctx, _name);
        } else {
            SerializerBase::baseError(err);
        }
    }
#endif
    template <typename F>
    ptrdiff_t run(char* _pbeg, unsigned _sz, F _f, ContextT& _rctx)
    {
        doPrepareRun(_pbeg, _sz);
        _f(*this, _rctx);
        return doRun(&_rctx);
    }

    template <typename F>
    std::ostream& run(std::ostream& _ros, F _f, ContextT& _rctx)
    {
        const size_t buf_cap = 8 * 1024;
        char         buf[buf_cap];
        ptrdiff_t    len;

        clear();

        doPrepareRun(buf, buf_cap);
        _f(*this, _rctx);
        len = doRun(&_rctx);

        while (len > 0) {
            _ros.write(buf, len);
            len = SerializerBase::run(buf, buf_cap, &_rctx);
        }
        return _ros;
    }

    ptrdiff_t run(char* _pbeg, unsigned _sz, ContextT& _rctx)
    {
        return SerializerBase::run(_pbeg, _sz, &_rctx);
    }

    const ErrorConditionT& error() const
    {
        return this->Base::error();
    }

    std::pair<ThisT&, ContextT&> wrap(ContextT& _rct)
    {
        return std::make_pair(std::ref(*this), std::ref(_rct));
    }

private:
    template <class Meta, class T>
    void addDispatch(const Meta& _meta, const T& _rt, ContextT& _rctx, const size_t _id, const char* const _name)
    {
        static_assert(!std::is_base_of_v<std::ostream, T>, "Cannot use std::ostream with Serializer");
        if constexpr (!is_shared_ptr_v<T> && !is_unique_ptr_v<T>) {
            static_assert(!std::is_pointer_v<T>, "Naked pointer are not supported - use std::shared_ptr or std::unique_ptr");
        }
        static_assert(!std::is_array_v<T>, "C style arrays not supported");
        static_assert(!std::is_floating_point_v<T>, "Floating point values not supported");

        if constexpr (std::is_base_of_v<std::istream, T>) {
            solid_assert(_meta.progress_function_);
            addStream(const_cast<T&>(_rt), _meta.size_, _meta.max_size_, _meta.progress_function_, _rctx, _id, _name);
        } else if constexpr (std::is_integral_v<T>) {
            addBasic(_rt, _name);
        } else if constexpr (is_bitset_v<T>) {
            addBitset(_rt, _name);
        } else if constexpr (is_shared_ptr_v<T> || is_unique_ptr_v<T>) {
            const auto* ptypemap = _meta.map();
            solid_assert(ptypemap != nullptr);
            const auto index_tuple = ptypemap->id(_rt.get());
            if constexpr (is_std_pair_v<TypeId>) {
                type_id_.first  = static_cast<decltype(type_id_.first)>(std::get<1>(index_tuple)); //the category
                type_id_.second = static_cast<decltype(type_id_.second)>(std::get<2>(index_tuple));
            } else {
                type_id_ = static_cast<decltype(type_id_)>(std::get<2>(index_tuple));
            }
            add(type_id_, _rctx, 1, "type_id");
            if (_rt) {
                ptypemap->reflect(*this, *_rt, _rctx, index_tuple);
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            addBasic(_rt, _meta.max_size_, _name);
        } else if constexpr (std::is_same_v<T, std::vector<char>>) {
            addVectorChar(_rt, _meta.max_size_, _name);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            addVectorBool(_rt, _meta.max_size_, _name);
        } else if constexpr (is_std_array_v<T>) {
            addArray(*this, _rt, _meta.size_, _rctx, _meta.max_size_, _name);
        } else if constexpr (std::is_array_v<T>) {

            //TODO:
        } else if constexpr (is_container_v<T>) {
            addContainer(*this, _rt, _meta.max_size_, _rctx, _name);
        } else {
            using namespace solid::reflection::v1;
            solidReflectV1(*this, _rt, _rctx);
        }
    }
};

template <class MetadataVariant, class MetadataFactory, class Context, typename TypeId>
inline std::ostream& operator<<(std::ostream& _ros, Serializer<MetadataVariant, MetadataFactory, Context, TypeId>& _rser)
{
    return _rser.SerializerBase::run(_ros);
}

template <typename S>
inline std::ostream& operator>>(std::ostream& _ros, std::pair<S&, typename S::ContextT&> _ser)
{
    return _ser.first.run(_ros, _ser.second);
}

} //namespace binary
} //namespace v3
} //namespace serialization
} //namespace solid
