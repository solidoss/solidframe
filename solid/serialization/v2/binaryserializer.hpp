#pragma once

#include "solid/serialization/v2/binarybase.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/function.hpp"
#include "solid/utility/innerlist.hpp"
#include <deque>
#include <ostream>
#include <vector>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

class SerializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(SerializerBase&, Runnable&, void*);

    using FunctionT = SOLID_FUNCTION<ReturnE(SerializerBase&, Runnable&, void*)>;

    struct Runnable : inner::Node<InnerListCount> {
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
        Runnable(const void* _ptr, CallbackT _call, F _f, const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(0)
            , data_(0)
            , name_(_name)
            , fnc_(_f)
        {
        }

        void clear()
        {
            ptr_  = nullptr;
            call_ = nullptr;
            SOLID_FUNCTION_CLEAR(fnc_);
        }

        const void* ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
    };

public:
    SerializerBase();

    std::ostream& run(std::ostream& _ros);
    long          run(char* _pbeg, unsigned _sz, void* _pctx = nullptr);

    void addBasic(const bool& _rb, const char* _name);
    void addBasic(const int8_t& _rb, const char* _name);
    void addBasic(const uint8_t& _rb, const char* _name);
    void addBasic(const std::string& _rb, const char* _name);

    template <typename T>
    void addBasic(const T& _rb, const char* _name)
    {
        idbg(_name << ' ' << _rb);
        Runnable r{nullptr, &store_cross, 0, static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (store_cross(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class S, class F>
    void addFunction(S& _rs, F _f, const char* _name)
    {
        idbg(_name);
        if (isRunEmpty() and _rs.pcrt_ != _rs.pend_) {
            _f(_rs, _name);
        } else {
            Runnable r{
                nullptr,
                call_function,
                [_f](SerializerBase& _rs, Runnable& _rr, void* _pctx) {
                    size_t old_sentinel = _rs.sentinel();

                    _f(static_cast<S&>(_rs), _rr.name_);

                    const bool is_run_empty = _rs.isRunEmpty();
                    _rs.sentinel(old_sentinel);
                    if (is_run_empty) {
                        return Base::ReturnE::Done;
                    } else {
                        _rr.call_ = noop;
                        return Base::ReturnE::Wait;
                    }
                },
                _name};
            schedule(std::move(r));
        }
    }

    template <class S, class F, class Ctx>
    void addFunction(S& _rs, F _f, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        if (isRunEmpty() and _rs.pcrt_ != _rs.pend_) {
            _f(_rs, _rctx, _name);
        } else {
            Runnable r{
                call_function,
                [_f](SerializerBase& _rs, Runnable& _rr, void* _pctx) {
                    size_t old_sentinel = _rs.sentinel();

                    _f(static_cast<S&>(_rs), *static_cast<Ctx*>(_pctx), _rr.name_);

                    const bool is_run_empty = _rs.isRunEmpty();
                    _rs.sentinel(old_sentinel);
                    if (is_run_empty) {
                        return Base::ReturnE::Done;
                    } else {
                        _rr.call_ = noop;
                        return Base::ReturnE::Wait;
                    }
                },
                _name};
            schedule(std::move(r));
        }
    }

    template <class S, class C>
    void addContainer(S& _rs, const C& _rc, const char* _name)
    {
        idbg(_name << ' ' << _rc.size());
        addBasic(_rc.size(), _name);
        if (_rc.size()) {
            typename C::const_iterator it = _rc.cbegin();

            while (_rs.pcrt_ != _rs.pend_ and it != _rc.cend()) {
                _rs.add(*it, _name);
                ++it;
            }

            if (it != _rc.cend()) {
                auto lambda = [it](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
                    const C& rcontainer = *static_cast<const C*>(_rr.ptr_);
                    S&       rs         = static_cast<S&>(_rs);

                    size_t old_sentinel = _rs.sentinel();

                    while (_rs.pcrt_ != _rs.pend_ and it != rcontainer.cend()) {
                        rs.add(*it, _rr.name_);
                        ++it;
                    }
                    const bool is_run_empty = _rs.isRunEmpty();
                    _rs.sentinel(old_sentinel);

                    if (it != rcontainer.cend() or not is_run_empty) {
                        return ReturnE::Wait;
                    } else {
                        return ReturnE::Done;
                    }
                };

                Runnable r{&_rc, call_function, lambda, _name};
                schedule(std::move(r));
            }
        }
    }

    template <class S, class C, class Ctx>
    void addContainer(S& _rs, const C& _rc, Ctx& _rctx, const char* _name)
    {
        idbg(_name << ' ' << _rc.size());

        addBasic(_rc.size(), _name);

        if (_rc.size()) {
            typename C::const_iterator it = _rc.cbegin();

            while (_rs.pcrt_ != _rs.pend_ and it != _rc.cend()) {
                _rs.add(*it, _rctx, _name);
                ++it;
            }

            if (it != _rc.cend()) {
                auto lambda = [it](SerializerBase& _rs, Runnable& _rr, void* _pctx) mutable {
                    const C& rcontainer = *static_cast<const C*>(_rr.ptr_);
                    Ctx&     rctx       = *static_cast<Ctx*>(_pctx);
                    S&       rs         = static_cast<S&>(_rs);

                    if (_rr.size_ == 0) {
                        _rr.data_ = _rs.sentinel();
                        ++_rr.size_;
                    }

                    while (_rs.pcrt_ != _rs.pend_ and it != rcontainer.cend()) {
                        rs.add(*it, rctx, _rr.name_);
                        ++it;
                    }

                    if (it != rcontainer.cend()) {
                        return ReturnE::Wait;
                    } else {
                        _rs.sentinel(_rr.data_);
                        return ReturnE::Done;
                    }
                };

                Runnable r{&_rc, call_function, lambda, _name};
                schedule(std::move(r));
            }
        }
    }

    template <class S, class T>
    void addPointer(S& _rs, const std::shared_ptr<T>& _rp, const char* _name)
    {
        idbg(_name);
    }

    template <class S, class T, class Ctx>
    void addPointer(S& _rs, const std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
    }
    template <class S, class T, class D>
    void addPointer(S& _rs, const std::unique_ptr<T, D>& _rp, const char* _name)
    {
        idbg(_name);
    }

    template <class S, class T, class D, class Ctx>
    void addPointer(S& _rs, const std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
    }

private:
    size_t schedule(Runnable&& _ur);

    size_t sentinel()
    {
        size_t olds = sentinel_;
        sentinel_   = run_lst_.frontIndex();
        idbg(olds << ' ' << sentinel_);
        return olds;
    }

    void sentinel(const size_t _s)
    {
        idbg(_s);
        sentinel_ = _s;
    }

    bool isRunEmpty() const
    {
        return sentinel_ == run_lst_.frontIndex();
    }

    static ReturnE store_byte(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_cross(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_binary(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE call_function(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE noop(SerializerBase& _rs, Runnable& _rr, void* _pctx);

private:
    enum {
        BufferCapacityE = sizeof(uint64_t) * 2
    };

    using RunVectorT = std::deque<Runnable>;
    using RunListT   = inner::List<RunVectorT, InnerListRun>;
    using CacheListT = inner::List<RunVectorT, InnerListCache>;

    char*      pbeg_;
    char*      pend_;
    char*      pcrt_;
    size_t     sentinel_;
    RunVectorT run_vec_;
    RunListT   run_lst_;
    CacheListT cache_lst_;
    char       buf_[BufferCapacityE];
};

template <class Ctx = void>
class Serializer;

template <>
class Serializer<void> : public SerializerBase {
public:
    using ThisT = Serializer<void>;

    template <typename T>
    ThisT& add(T& _rt, const char* _name)
    {
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(const T& _rt, const char* _name)
    {
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }
};

inline std::ostream& operator<<(std::ostream& _ros, Serializer<>& _rser)
{
    return _rser.run(_ros);
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
