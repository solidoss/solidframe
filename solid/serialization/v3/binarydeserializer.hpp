// solid/serialization/v3/binarydeserializer.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
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

#include "solid/serialization/v3/binarybase.hpp"
#include "solid/serialization/v3/binarybasic.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/convertors.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"

namespace solid {
namespace serialization {
namespace v3 {
namespace binary {

class DeserializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(DeserializerBase&, Runnable&, void*);

    using FunctionT = solid_function_t(ReturnE(DeserializerBase&, Runnable&, void*));

    struct Runnable {
        Runnable(
            void*       _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
            , limit_(0)
        {
        }

        Runnable(
            void*       _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            uint64_t    _limit,
            const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
            , limit_(_limit)
        {
        }

        template <class F>
        Runnable(void* _ptr, CallbackT _call, const char* _name, F&& _f)
            : ptr_(_ptr)
            , call_(_call)
            , size_(0)
            , data_(0)
            , name_(_name)
            , fnc_(std::move(_f))
            , limit_(0)
        {
        }

        template <class F>
        Runnable(
            void*       _ptr,
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
            , limit_(0)
        {
        }

        void clear()
        {
            ptr_  = nullptr;
            call_ = nullptr;
            solid_function_clear(fnc_);
        }

        void*       ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
        uint64_t    limit_;
    };

    using RunListT         = std::list<Runnable>;
    using RunListIteratorT = std::list<Runnable>::const_iterator;

protected:
    DeserializerBase(const reflection::v1::TypeMapBase* const _ptype_map);

public:
    static constexpr bool is_const_reflector = false;

public:
    std::istream& run(std::istream& _ris, void* _pctx = nullptr);
    ptrdiff_t     run(const char* _pbeg, unsigned _sz, void* _pctx = nullptr);

    void clear();

    const ErrorConditionT& error() const
    {
        return Base::error();
    }

    bool empty() const
    {
        return run_lst_.empty();
    }

    inline void addBasic(bool& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_bool, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadBool(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(int8_t& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_byte, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(uint8_t& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_byte, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <typename A>
    inline void addVectorBool(std::vector<bool, A>& _rv, const uint64_t _limit, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rv, &load_vector_bool<A>, 0, 0, _limit, _name};

        if (isRunEmpty()) {
            if (load_vector_bool<A>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <size_t N>
    inline void addBitset(std::bitset<N>& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_bitset<N>, 0, 0, _name};

        _rb.reset();

        if (isRunEmpty()) {
            if (load_bitset<N>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class T>
    void addBasic(T& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_cross<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (doLoadCross<T>(r) == ReturnE::Done) {
                return;
            }
        }
        schedule(std::move(r));
    }

    template <class T>
    void addBasicWithCheck(T& _rb, const char* _name)
    {
        solid_dbg(logger, Info, _name);

        Runnable r{&_rb, &load_cross_with_check<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (load_cross_with_check<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    void addBasic(std::string& _rb, const uint64_t _limit, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_string, 0, 0, _limit, _name};

        _rb.clear(); //necessary otherwise map<string, something> would not work on gcc5.3

        if (isRunEmpty()) {
            if (doLoadString(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <typename T, class A>
    void addVectorChar(std::vector<T, A>& _rb, const uint64_t _limit, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        Runnable r{&_rb, &load_vector_char<T, A>, 0, 0, _limit, _name};

        _rb.clear(); //necessary otherwise map<string, something> would not work on gcc5.3

        if (isRunEmpty()) {
            if (load_vector_char<T, A>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class D, class F, class Ctx>
    void addFunction(D& _rd, F&& _f, Ctx& _rctx, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        if (isRunEmpty()) {
            _f(_rd, _rctx);
        } else {
            Runnable r{
                nullptr,
                call_function,
                _name,
                [_f = std::move(_f)](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
                    const RunListIteratorT old_sentinel = _rd.sentinel();

                    _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx));

                    const bool is_run_empty = _rd.isRunEmpty();
                    _rd.sentinel(old_sentinel);
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

    template <class D, class F>
    void pushFunction(D& _rd, F _f, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        auto lambda = [_f = std::move(_f)](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            const RunListIteratorT old_sentinel = _rd.sentinel();
            const bool             done         = _f(static_cast<D&>(_rd), _rr.name_);
            const bool             is_run_empty = _rd.isRunEmpty();

            _rd.sentinel(old_sentinel);

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
        Runnable r{nullptr, call_function, lambda, _name};

        tryRun(std::move(r));
    }

    template <class D, class F, class Ctx>
    void pushFunction(D& _rd, F _f, Ctx& _rctx, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        auto lambda = [_f = std::move(_f)](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            const RunListIteratorT old_sentinel = _rd.sentinel();
            const bool             done         = _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);
            const bool             is_run_empty = _rd.isRunEmpty();

            _rd.sentinel(old_sentinel);

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
        Runnable r{nullptr, call_function, lambda, _name};

        tryRun(std::move(r), &_rctx);
    }

    template <class D, class C>
    void addContainer(D& _rd, C& _rc, const uint64_t _limit, const char* _name)
    {
        solid_dbg(logger, Info, _name);

        typename C::value_type value;
        bool                   init          = true;
        bool                   parsing_value = false;
        auto                   lambda        = [value, parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            C& rcontainer = *static_cast<C*>(_rr.ptr_);
            D& rd         = static_cast<D&>(_rd);

            if (init) {
                init                                = false;
                const RunListIteratorT old_sentinel = _rd.sentinel();
                solid_assert_log(_rd.isRunEmpty(), logger);

                rd.addBasicWithCheck(_rr.size_, _rr.name_);

                const bool is_run_empty = _rd.isRunEmpty();
                _rd.sentinel(old_sentinel);
                if (!is_run_empty) {
                    return ReturnE::Wait;
                }
            }

            if (parsing_value) {
                rcontainer.insert(rcontainer.end(), std::move(value));
                parsing_value = false;
            } else if (_rr.size_ > _rr.limit_) {
                _rd.baseError(error_limit_container);
                return ReturnE::Done;
            }

            const RunListIteratorT old_sentinel = _rd.sentinel();

            while (_rd.pcrt_ != _rd.pend_ && _rr.size_ != 0) {
                rd.add(value, _rr.name_);
                --_rr.size_;

                if (_rd.isRunEmpty()) {
                    //the value was parsed
                    rcontainer.insert(rcontainer.end(), std::move(value));
                } else {
                    parsing_value = true;
                    solid_check_log(_rd.pcrt_ == _rd.pend_, logger, "buffer not empty");
                }
            }

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);

            if (_rr.size_ == 0 && is_run_empty) {
                return ReturnE::Done;
            }
            return ReturnE::Wait;
        };

        Runnable r{&_rc, call_function, lambda, _name};
        r.limit_ = _limit;

        tryRun(std::move(r));
    }

    template <class D, class C, class Ctx>
    void addContainer(D& _rd, C& _rc, const uint64_t _limit, Ctx& _rctx, const char* _name)
    {
        solid_dbg(logger, Info, _name);

        typename C::value_type value{};
        bool                   init          = true;
        bool                   parsing_value = false;
        auto                   lambda        = [value = std::move(value), parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            C&   rcontainer = *static_cast<C*>(_rr.ptr_);
            D&   rd         = static_cast<D&>(_rd);
            Ctx& rctx       = *static_cast<Ctx*>(_pctx);

            if (init) {
                init                                = false;
                const RunListIteratorT old_sentinel = _rd.sentinel();
                solid_assert_log(_rd.isRunEmpty(), logger);

                rd.addBasicWithCheck(_rr.size_, _rr.name_);

                const bool is_run_empty = _rd.isRunEmpty();
                _rd.sentinel(old_sentinel);
                if (!is_run_empty) {
                    return ReturnE::Wait;
                }
            }

            if (parsing_value) {
                rcontainer.insert(rcontainer.end(), std::move(value));
                parsing_value = false;
            } else if (_rr.size_ > _rr.limit_) {
                _rd.baseError(error_limit_container);
                return ReturnE::Done;
            }

            const RunListIteratorT old_sentinel = _rd.sentinel();

            while (_rd.pcrt_ != _rd.pend_ && _rr.size_ != 0) {
                rd.add(value, rctx, 0, _rr.name_); //TODO: use a propper index
                --_rr.size_;

                if (_rd.isRunEmpty()) {
                    //the value was parsed
                    rcontainer.insert(rcontainer.end(), std::move(value));
                } else {
                    parsing_value = true;
                    solid_check_log(_rd.pcrt_ == _rd.pend_, logger, "buffer not empty");
                }
            }

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);

            if (_rr.size_ == 0 && is_run_empty) {
                return ReturnE::Done;
            }
            return ReturnE::Wait;
        };

        Runnable r{&_rc, call_function, _name, lambda};
        r.limit_ = _limit;

        tryRun(std::move(r), &_rctx);
    }

    template <class F, class Ctx>
    void addStream(std::ostream& _ros, const uint64_t _limit, F _f, Ctx& _rctx, const size_t _id, const char* _name)
    {
        uint64_t len    = 0;
        auto     lambda = [_f = std::move(_f), len, _limit, _id](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            std::ostream& ros  = *const_cast<std::ostream*>(static_cast<const std::ostream*>(_rr.ptr_));
            Ctx&          rctx = *static_cast<Ctx*>(_pctx);
            len += _rr.data_;

            _f(rctx, ros, len, _rr.data_ == 0, _id, _rr.name_);

            if (len > _limit) {
                _rd.baseError(error_limit_stream);
            }
            return ReturnE::Done;
        };

        Runnable r{&_ros, &load_stream, 0, 0, _name, lambda};
        if (isRunEmpty()) {
            if (load_stream(*this, r, &_rctx) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    void addBinary(void* _pv, const size_t _sz, const char* _name)
    {
        Runnable r{_pv, &load_binary, _sz, 0, _name};

        if (isRunEmpty()) {
            if (doLoadBinary(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class D, class T, size_t N, class C>
    void addArray(D& _rd, std::array<T, N>& _rc, const size_t _max_size, C& _rctx, const char* _name)
    {
        _rd.addBasicWithCheck(data_.u64_, _name);

        {
            Runnable r{&_rc, load_array_start<D, T, N, C>, 0, 0, _max_size, _name};

            tryRun(std::move(r), &_rctx);
        }
    }

    template <class T>
    void addBlob(void* _pv, T& _rsz, const size_t _cp, const char* _name)
    {

        addBasicWithCheck(data_.u64_, _name);
#if 0
        {
            Runnable r{&_rsz, &load_array_size<T>, 0, 0, _name};
            if (isRunEmpty()) {
                load_array_size<T>(*this, r, nullptr);
            } else {
                schedule(std::move(r));
            }
        }
#endif
        {
            Runnable r{_pv, &load_blob, _cp, 0, _name};

            if (isRunEmpty()) {
                if (load_blob(*this, r, nullptr) == ReturnE::Done) {
                    return;
                }
            }

            schedule(std::move(r));
        }
    }

protected:
    void doPrepareRun(const char* _pbeg, unsigned _sz)
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
    friend class TypeMapBase;
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

    static ReturnE load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_byte(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_version(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

    template <typename T>
    static ReturnE load_cross_data(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        return _rd.doLoadCrossData<T>(_rr);
    }

    template <typename T>
    inline static ReturnE load_cross(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        return _rd.doLoadCross<T>(_rr);
    }

    //NOTE: load_cross_with_check should not modify _rr.ptr_!!
    template <typename T>
    static ReturnE load_cross_with_check(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        if (_rd.pcrt_ != _rd.pend_) {
            if (_rr.size_ == 0) {
                //first run
                uint64_t    v;
                const char* p = cross::load_with_check(_rd.pcrt_, _rd.pend_ - _rd.pcrt_, v);

                if (p) {
                    _rd.pcrt_ = p;
                    T vt      = static_cast<T>(v);

                    if (static_cast<uint64_t>(vt) == v) {
                        *reinterpret_cast<T*>(_rr.ptr_) = vt;
                    } else {
                        _rd.baseError(error_cross_integer);
                    }

                    return ReturnE::Done;
                } else {
                    //not enough data
                    _rr.size_ = cross::size(_rd.pcrt_);

                    if (_rr.size_ == InvalidSize()) {
                        _rd.baseError(error_cross_integer);
                        return ReturnE::Done;
                    }

                    ++_rd.pcrt_; //skip the size char
                    --_rr.size_;

                    size_t toread = _rd.pend_ - _rd.pcrt_;
                    solid_check_log(toread <= _rr.size_, logger, "Should not happen");
                    memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
                    _rd.pcrt_ += toread;
                    _rr.size_ -= toread;
                    _rr.data_ += toread;
                    return ReturnE::Wait;
                }
            } else {
                size_t toread = _rd.pend_ - _rd.pcrt_;
                solid_check_log(toread >= _rr.size_, logger, "Should not happen");
                if (toread > _rr.size_) {
                    toread = static_cast<size_t>(_rr.size_);
                }
                memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
                _rd.pcrt_ += toread;
                _rr.size_ -= toread;
                _rr.data_ += toread;
                if (_rr.size_ == 0) {
                    uint64_t    v;
                    T           vt;
                    const char* p = cross::load_without_check(_rd.data_.buf_, static_cast<size_t>(_rr.data_), v);
                    if (p == nullptr) {
                        _rd.baseError(error_cross_integer);
                        return ReturnE::Done;
                    }

                    vt = static_cast<T>(v);

                    if (static_cast<uint64_t>(vt) == v) {
                        *reinterpret_cast<T*>(_rr.ptr_) = vt;
                    } else {
                        _rd.baseError(error_cross_integer);
                    }

                    return ReturnE::Done;
                }
                return ReturnE::Wait;
            }
        }
        return ReturnE::Wait;
    }

    static ReturnE load_string(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

    static ReturnE noop(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

    static ReturnE load_stream(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk_begin(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

    template <typename A>
    static ReturnE load_vector_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        void* pstr      = _rr.ptr_;
        _rr.ptr_        = &_rd.data_.u64_;
        const ReturnE r = load_cross_with_check<uint64_t>(_rd, _rr, nullptr);
        _rr.ptr_        = pstr;

        if (r == ReturnE::Done && _rd.data_.u64_ != 0) {
            _rr.size_ = _rd.data_.u64_;
            solid_dbg(logger, Info, "size = " << _rr.size_ << ' ' << _rr.limit_);

            if (_rr.size_ > _rr.limit_) {
                _rd.baseError(error_limit_container);
                return ReturnE::Done;
            }

            _rr.data_ = 0;
            _rr.call_ = load_vector_bool_continue<A>;
            return load_vector_bool_continue<A>(_rd, _rr, _pctx);
        }
        return r;
    }

    template <typename A>
    static ReturnE load_vector_bool_continue(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {

        if (_rd.pcrt_ != _rd.pend_) {

            size_t toread = (_rd.pend_ - _rd.pcrt_) << 3;

            if (toread > _rr.size_) {
                toread = static_cast<size_t>(_rr.size_);
            }

            std::vector<bool, A>& vec = *reinterpret_cast<std::vector<bool, A>*>(_rr.ptr_);

            for (size_t i = 0; i < toread; ++i) {
                vec.push_back(load_bit_from(reinterpret_cast<const uint8_t*>(_rd.pcrt_), i));
            }

            _rd.pcrt_ += (toread >> 3);

            if ((toread & 7) != 0) {
                ++_rd.pcrt_;
            }
            _rr.size_ -= toread;
            _rr.data_ += toread;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }

    template <size_t N>
    static ReturnE load_bitset(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        void* pstr      = _rr.ptr_;
        _rr.ptr_        = &_rd.data_.u64_;
        const ReturnE r = load_cross_with_check<uint64_t>(_rd, _rr, nullptr);
        _rr.ptr_        = pstr;

        if (r == ReturnE::Done && _rd.data_.u64_ != 0) {
            _rr.size_ = _rd.data_.u64_;
            solid_dbg(logger, Info, "size = " << _rr.size_);

            if (_rr.size_ > N) {
                _rd.baseError(error_limit_container);
                return ReturnE::Done;
            }

            _rr.data_ = 0;
            _rr.call_ = load_bitset_continue<N>;
            return load_bitset_continue<N>(_rd, _rr, _pctx);
        }
        return r;
    }

    template <size_t N>
    static ReturnE load_bitset_continue(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {

        if (_rd.pcrt_ != _rd.pend_) {

            size_t toread = (_rd.pend_ - _rd.pcrt_) << 3;

            if (toread > _rr.size_) {
                toread = static_cast<size_t>(_rr.size_);
            }

            std::bitset<N>& vec = *reinterpret_cast<std::bitset<N>*>(_rr.ptr_);

            for (size_t i = 0; i < toread; ++i) {
                vec.set(static_cast<size_t>(_rr.data_ + i), load_bit_from(reinterpret_cast<const uint8_t*>(_rd.pcrt_), i));
            }

            _rd.pcrt_ += (toread >> 3);

            if ((toread & 7) != 0) {
                ++_rd.pcrt_;
            }
            _rr.size_ -= toread;
            _rr.data_ += toread;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }

    template <typename T, class A>
    static Base::ReturnE load_vector_char(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        solid_dbg(logger, Info, _rr.name_);
        //_rr.ptr_ contains pointer to string object
        void* pstr      = _rr.ptr_;
        _rr.ptr_        = &_rd.data_.u64_;
        const ReturnE r = load_cross_with_check<uint64_t>(_rd, _rr, nullptr);
        _rr.ptr_        = pstr;

        if (r == ReturnE::Done && _rd.data_.u64_ != 0) {
            _rr.size_ = _rd.data_.u64_;
            solid_dbg(logger, Info, "size = " << _rr.size_);

            if (_rr.size_ > _rr.limit_) {
                _rd.baseError(error_limit_string);
                return ReturnE::Done;
            }

            std::vector<T, A>& rstr = *static_cast<std::vector<T, A>*>(pstr);
            rstr.resize(static_cast<size_t>(_rr.size_));
            _rr.ptr_  = const_cast<char*>(reinterpret_cast<const char*>(rstr.data()));
            _rr.data_ = 0;
            _rr.call_ = load_binary;
            return _rd.doLoadBinary(_rr);
        }
        return r;
    }

    template <class D, class T, size_t N, class C>
    static ReturnE load_array_start(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        if (_rr.limit_ != 0 && _rd.data_.u64_ > _rr.limit_) {
            _rd.baseError(error_limit_array);
            return ReturnE::Done;
        }

        _rr.size_ = _rd.data_.u64_;
        _rr.data_ = 0;
        _rr.call_ = load_array_continue<D, T, N, C>;
        return load_array_continue<D, T, N, C>(_rd, _rr, _pctx);
    }

    template <class D, class T, size_t N, class Ctx>
    static ReturnE load_array_continue(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        std::array<T, N>& rcontainer = *static_cast<std::array<T, N>*>(_rr.ptr_);
        D&                rd         = static_cast<D&>(_rd);
        Ctx&              rctx       = *static_cast<Ctx*>(_pctx);

        const RunListIteratorT old_sentinel = _rd.sentinel();

        while (_rd.pcrt_ != _rd.pend_ && _rr.data_ < _rr.size_) {
            rd.add(rcontainer[static_cast<size_t>(_rr.data_)], rctx, 0, _rr.name_); //TODO: add propper index
            ++_rr.data_;
        }

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);

        if (_rr.data_ == _rr.size_ && is_run_empty) {
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    }

    static ReturnE load_blob(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        if (_rd.data_.u64_ > _rr.size_) {
            _rd.baseError(error_limit_blob);
            return ReturnE::Done;
        }

        _rr.call_ = load_binary;
        _rr.size_ = _rd.data_.u64_;

        return _rd.doLoadBinary(_rr);
    }

private:
    inline Base::ReturnE doLoadBinary(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            size_t toread = pend_ - pcrt_;
            if (toread > _rr.size_) {
                toread = static_cast<size_t>(_rr.size_);
            }
            memcpy(_rr.ptr_, pcrt_, toread);
            pcrt_ += toread;
            _rr.size_ -= toread;
            _rr.ptr_ = reinterpret_cast<uint8_t*>(_rr.ptr_) + toread;
            return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
        }
        return ReturnE::Wait;
    }
    inline Base::ReturnE doLoadBool(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            bool* pb = static_cast<bool*>(_rr.ptr_);
            *pb      = *reinterpret_cast<const uint8_t*>(pcrt_) == 0xFF ? true : false;
            ++pcrt_;
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    }

    inline Base::ReturnE doLoadByte(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            uint8_t* pb = static_cast<uint8_t*>(_rr.ptr_);
            *pb         = *reinterpret_cast<const uint8_t*>(pcrt_);
            ++pcrt_;
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    }
    inline Base::ReturnE doLoadString(Runnable& _rr)
    {
        solid_dbg(logger, Info, _rr.name_);
        //_rr.ptr_ contains pointer to string object
        void* pstr      = _rr.ptr_;
        _rr.ptr_        = &data_.u64_;
        const ReturnE r = load_cross_with_check<uint64_t>(*this, _rr, nullptr);
        _rr.ptr_        = pstr;

        if (r == ReturnE::Done && data_.u64_ != 0) {
            _rr.size_ = data_.u64_;

            if (_rr.size_ > _rr.limit_) {
                baseError(error_limit_string);
                return ReturnE::Done;
            }

            std::string& rstr = *static_cast<std::string*>(pstr);
            rstr.resize(static_cast<size_t>(_rr.size_));
            _rr.ptr_  = const_cast<char*>(rstr.data());
            _rr.data_ = 0;
            _rr.call_ = load_binary;
            return doLoadBinary(_rr);
        }
        return r;
    }

    template <typename T>
    inline ReturnE doLoadCrossData(Runnable& _rr)
    {
        if (pcrt_ != pend_) {
            size_t toread = pend_ - pcrt_;
            if (toread > _rr.size_) {
                toread = static_cast<size_t>(_rr.size_);
            }

            memcpy(data_.buf_ + _rr.data_, pcrt_, toread);

            _rr.size_ -= toread;
            _rr.data_ += toread;
            pcrt_ += toread;

            if (_rr.size_ == 0) {
#ifdef SOLID_ON_BIG_ENDIAN
                const uint64_t v = swap_bytes(data_.u64_);
#else
                const uint64_t v = data_.u64_;
#endif
                const T vt = static_cast<T>(v);
                solid_dbg(logger, Info, "vt = " << vt);

                if (static_cast<uint64_t>(vt) == v) {
                    *reinterpret_cast<T*>(_rr.ptr_) = vt;
                } else {
                    baseError(error_cross_integer);
                }
                return ReturnE::Done;
            }
        }
        return ReturnE::Wait;
    }

    template <typename T>
    inline ReturnE doLoadCross(Runnable& _rr)
    {
#if 1
        if (pcrt_ != pend_) {
            _rr.size_ = *pcrt_;
            solid_dbg(logger, Info, "sz = " << _rr.size_ << " c = " << (int)*pcrt_);
            ++pcrt_;

            if (_rr.size_ > sizeof(uint64_t)) {
                baseError(error_cross_integer);
                return ReturnE::Done;
            } else if (_rr.size_ == 0) {
                *reinterpret_cast<T*>(_rr.ptr_) = 0;
                return ReturnE::Done;
            }
            _rr.call_  = load_cross_data<T>;
            data_.u64_ = 0;
            return doLoadCrossData<T>(_rr);
        }
        return ReturnE::Wait;
#else
        _rr.size_ = sizeof(T);
        _rr.call_ = load_cross_data<T>;
        data_.u64_ = 0;
        return doLoadCrossData<T>(_rr);
#endif
    }

protected:
    enum {
        BufferCapacityE = (sizeof(uint64_t) * 1)
    };
    union {
        char     buf_[BufferCapacityE];
        uint64_t u64_;
        void*    p_;
    } data_;
    const reflection::v1::TypeMapBase* const ptype_map_;

private:
    const char*      pbeg_;
    const char*      pend_;
    const char*      pcrt_;
    RunListT         run_lst_;
    RunListIteratorT sentinel_;
}; // namespace solid

template <class MetadataVariant, class MetadataFactory, class Context, typename TypeId>
class Deserializer : public DeserializerBase {
    TypeId                 type_id_;
    const MetadataFactory& rmetadata_factory_;

public:
    using ContextT = Context;
    using ThisT    = Deserializer<MetadataVariant, MetadataFactory, Context, TypeId>;

    Deserializer(
        MetadataFactory& _rmetadata_factory, const reflection::v1::TypeMapBase& _rtype_map)
        : DeserializerBase(&_rtype_map)
        , rmetadata_factory_(_rmetadata_factory)
    {
    }

    Deserializer(
        MetadataFactory& _rmetadata_factory)
        : DeserializerBase(nullptr)
        , rmetadata_factory_(_rmetadata_factory)
    {
    }

    template <typename T, typename F>
    auto& add(T& _rt, Context& _rctx, const size_t _id, const char* const _name, F _f)
    {
        auto meta = rmetadata_factory_(_rt, _rctx, this->ptype_map_);
        _f(meta);

        addDispatch(meta, _rt, _rctx, _id, _name);
        return *this;
    }

    template <typename T>
    auto& add(T& _rt, Context& _rctx, const size_t _id, const char* const _name)
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
    ThisT& add(std::ostream& _ros, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ros, limits().stream(), _f, _rctx, _name);
        return *this;
    }

    template <typename F>
    ThisT& add(std::ostream& _ros, const Limit _limit, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ros, _limit.value_, _f, _rctx, _name);
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

    template <typename T, size_t N, typename S>
    ThisT& add(std::array<T, N>& _rt, S& _rsz, Ctx& _rctx, const char* _name)
    {
        addArray(*this, _rt, _rsz, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(void* _pv, T& _rsz, const size_t _cp, Ctx& /*_rctx*/, const char* _name)
    {
        addBlob(_pv, _rsz, _cp, _name);
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
    void addPointer(std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        add(type_id_, _rctx, _name);
        add([&_rp](ThisT& _rd, Ctx& _rctx, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _rctx, _name);
        },
            _rctx, _name);
    }

    template <class T, class D>
    void addPointer(std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        solid_dbg(logger, Info, _name);
        add(type_id_, _rctx, _name);
        add([&_rp](ThisT& _rd, Ctx& _rctx, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _rctx, _name);
        },
            _rctx, _name);
    }
#endif

    std::istream& run(std::istream& _ris, ContextT& _rctx)
    {
        return DeserializerBase::run(_ris, &_rctx);
    }

    std::pair<ThisT&, ContextT&> wrap(ContextT& _rct)
    {
        return std::make_pair(std::ref(*this), std::ref(_rct));
    }

    template <typename F>
    ptrdiff_t run(const char* _pbeg, unsigned _sz, F _f, ContextT& _rctx)
    {
        doPrepareRun(_pbeg, _sz);
        _f(*this, _rctx);
        return doRun(&_rctx);
    }

    ptrdiff_t run(const char* _pbeg, unsigned _sz, ContextT& _rctx)
    {
        return DeserializerBase::run(_pbeg, _sz, &_rctx);
    }

    template <typename F>
    std::istream& run(std::istream& _ris, F _f, ContextT& _rctx)
    {
        const size_t buf_cap = 8 * 1024;
        char         buf[buf_cap];

        clear();
        _ris.read(buf, buf_cap);
        std::streamsize readsz = _ris.gcount();

        if (readsz) {
            doPrepareRun(buf, static_cast<size_t>(readsz));

            _f(*this, _rctx);

            if (readsz == doRun(&_rctx)) {
                do {
                    _ris.read(buf, buf_cap);
                    readsz = _ris.gcount();
                } while (readsz && (readsz == DeserializerBase::run(buf, static_cast<unsigned>(readsz), &_rctx)));
            }
        }

        return _ris;
    }

    const ErrorConditionT& error() const
    {
        return Base::error();
    }

private:
    template <class Meta, class T>
    void addDispatch(const Meta& _meta, T& _rt, ContextT& _rctx, const size_t _id, const char* const _name)
    {
        static_assert(!std::is_base_of_v<std::istream, T>, "Cannot use std::istream with Deserializer");
        if constexpr (!is_shared_ptr_v<T> && !is_unique_ptr_v<T>) {
            static_assert(!std::is_pointer_v<T>, "Naked pointer are not supported - use std::shared_ptr or std::unique_ptr");
        }
        static_assert(!std::is_array_v<T>, "C style arrays not supported");
        static_assert(!std::is_floating_point_v<T>, "Floating point values not supported");

        if constexpr (std::is_base_of_v<std::ostream, T>) {
            addStream(const_cast<T&>(_rt), _meta.max_size_, _meta.progress_function_, _rctx, _id, _name);
        } else if constexpr (std::is_integral_v<T>) {
            addBasic(_rt, _name);
        } else if constexpr (is_bitset_v<T>) {
            addBitset(_rt, _name);
        } else if constexpr (is_shared_ptr_v<T> || is_unique_ptr_v<T>) {
            const auto* ptypemap = _meta.map();
            solid_assert(ptypemap != nullptr);
            add(type_id_, _rctx, 1, "type_id");
            add(
                [ptypemap, &_rt, this](ThisT& _rthis, ContextT& _rctx) {
                    size_t category = 0;
                    size_t index;
                    if constexpr (is_std_pair_v<TypeId>) {
                        category = static_cast<size_t>(type_id_.first);
                        index    = static_cast<size_t>(type_id_.second);
                    } else {
                        index = static_cast<size_t>(type_id_);
                    }
                    if (index != 0) {
                        const auto rv = ptypemap->createAndReflect(_rthis, _rt, _rctx, category, index);
                        if (rv == 0 && index != 0) {
                            baseError(error_unknown_type);
                        }
                    } else {
                        //nullptr
                        _rt.reset();
                    }
                },
                _rctx);

        } else if constexpr (std::is_same_v<T, std::string>) {
            addBasic(_rt, _meta.max_size_, _name);
        } else if constexpr (std::is_same_v<T, std::vector<char>>) {
            addVectorChar(_rt, _meta.max_size_, _name);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            addVectorBool(_rt, _meta.max_size_, _name);
        } else if constexpr (is_std_array_v<T>) {
            addArray(*this, _rt, _meta.max_size_, _rctx, _name);
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
inline std::istream& operator>>(std::istream& _ris, Deserializer<MetadataFactory, MetadataFactory, Context, TypeId>& _rdes)
{
    return _rdes.DeserializerBase::run(_ris);
}

template <typename D>
inline std::istream& operator>>(std::istream& _ris, std::pair<D&, typename D::ContextT&> _des)
{
    return _des.first.run(_ris, _des.second);
}

} // namespace binary
} // namespace v3
} // namespace serialization
} // namespace solid
