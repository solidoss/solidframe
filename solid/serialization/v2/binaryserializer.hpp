#pragma once

#include "solid/serialization/v2/binarybase.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/debug.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/innerlist.hpp"
#include "solid/utility/ioformat.hpp"
#include <deque>
#include <istream>
#include <ostream>
#include <vector>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

class SerializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(SerializerBase&, Runnable&, void*);

    using FunctionT = SOLID_FUNCTION(ReturnE(SerializerBase&, Runnable&, void*));

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
        Runnable(const void* _ptr, CallbackT _call, F&& _f, const char* _name)
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
            F&&         _f,
            const char* _name)
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
    static constexpr bool is_serializer   = true;
    static constexpr bool is_deserializer = false;

    std::ostream& run(std::ostream& _ros);
    long          run(char* _pbeg, unsigned _sz, void* _pctx = nullptr);

    void clear();

    void limits(const Limits& _rlimits);

    const Limits& limits() const
    {
        return Base::limits();
    }

public: //should be protected
    SerializerBase();

    inline void addBasic(const bool& _rb, const char* _name)
    {
        idbg(_name);
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
        idbg(_name);
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
        idbg(_name);
        Runnable r{nullptr, &store_byte, 1, static_cast<uint64_t>(_rb), _name};

        if (isRunEmpty()) {
            if (doStoreByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(const std::string& _rb, const char* _name)
    {
        idbg(_name << ' ' << _rb.size() << ' ' << trim_str(_rb.c_str(), _rb.size(), 4, 4));

        if (limits().hasString() && _rb.size() > limits().string()) {
            error(error_limit_string);
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
        idbg(_name << ' ' << _rb);
#if 0
        Runnable r{std::addressof(_rb), &store_binary, sizeof(T), static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreBinary(r) == ReturnE::Done) {
                return;
            }
        }
#elif 1
        Runnable r{std::addressof(_rb), &store_cross, 0, static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreCross(r) == ReturnE::Done) {
                return;
            }
        }
#else
        Runnable r{nullptr, &store_cross_with_check, 0, static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreCrossWithCheck(r) == ReturnE::Done) {
                return;
            }
        }
#endif
        schedule(std::move(r));
    }
    template <typename T>
    inline void addBasicWithCheck(const T& _rb, const char* _name)
    {
        idbg(_name << ' ' << _rb);
        Runnable r{nullptr, &store_cross_with_check, 0, static_cast<uint64_t>(_rb), _name};
        if (isRunEmpty()) {
            if (doStoreCrossWithCheck(r) == ReturnE::Done) {
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

    template <class S, class F>
    void pushFunction(S& _rs, F&& _f, const char* _name)
    {
        idbg(_name);
        auto lambda = [_f = std::move(_f)](SerializerBase & _rs, Runnable & _rr, void* _pctx) mutable
        {
            size_t     old_sentinel = _rs.sentinel();
            const bool done         = _f(static_cast<S&>(_rs), _rr.name_);

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
    void pushFunction(S& _rs, F&& _f, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        auto lambda = [_f = std::move(_f)](SerializerBase & _rs, Runnable & _rr, void* _pctx) mutable
        {
            size_t     old_sentinel = _rs.sentinel();
            const bool done         = _f(static_cast<S&>(_rs), *static_cast<Ctx*>(_pctx), _rr.name_);

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

    template <class S, class C>
    void addContainer(S& _rs, const C& _rc, const char* _name)
    {
        idbg(_name << ' ' << _rc.size());
        if (_rc.size() > limits().container()) {
            error(error_limit_container);
            return;
        }

        addBasicWithCheck(_rc.size(), _name);

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

        if (_rc.size() > limits().container()) {
            error(error_limit_container);
            return;
        }

        addBasicWithCheck(_rc.size(), _name);

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

    template <class F>
    void addStream(std::istream& _ris, const uint64_t _sz, F _f, const char* _name)
    {
        auto lambda = [_f = std::move(_f)](SerializerBase & _rs, Runnable & _rr, void* _pctx)
        {
            std::istream& ris = *const_cast<std::istream*>(static_cast<const std::istream*>(_rr.ptr_));
            _f(ris, _rr.data_, _rr.size_ == 0, _rr.name_);
            return ReturnE::Done;
        };

        Runnable r{&_ris, &store_stream, _sz, 0, lambda, _name};

        if (isRunEmpty()) {
            if (store_stream(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

private:
    void tryRun(Runnable&& _ur, void* _pctx = nullptr);

    size_t schedule(Runnable&& _ur);

    size_t sentinel()
    {
        size_t olds = sentinel_;
        sentinel_   = run_lst_.frontIndex();
        idbg(olds << ' ' << sentinel_);
        return olds;
    }

    void error(const ErrorConditionT& _err)
    {
        if (!error_) {
            error_ = _err;
            pcrt_ = pbeg_ = pend_ = nullptr;
        }
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
    static ReturnE store_cross_with_check(SerializerBase& _rs, Runnable& _rr, void* _pctx);
    static ReturnE store_binary(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE call_function(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE noop(SerializerBase& _rs, Runnable& _rr, void* _pctx);

    static ReturnE store_stream(SerializerBase& _rs, Runnable& _rr, void* _pctx);

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
                towrite = _rr.size_;
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
                SOLID_CHECK(p, "should not be null");
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
        if (pcrt_ != pend_) {
            const size_t sz = max_padded_byte_cout(_rr.data_);
            *pcrt_          = static_cast<char>(sz);
            idbg("sz = " << sz << " c = " << (int)*pcrt_);
            ++pcrt_;
            _rr.size_ = sz;
            _rr.call_ = store_binary;
            _rr.ptr_  = &_rr.data_;
            return doStoreBinary(_rr);
        }
        return ReturnE::Wait;
    }

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
    union {
        char     buf_[BufferCapacityE];
        uint64_t u64_;
        void*    p_;
    } data_;
}; // namespace v2

template <class Ctx = void>
class Serializer;

template <>
class Serializer<void> : public SerializerBase {
public:
    using ThisT = Serializer<void>;

    template <typename F>
    ThisT& add(std::istream& _ris, const uint64_t _sz, F _f, const char* _name)
    {
        addStream(_ris, _sz, _f, _name);
        return *this;
    }
    template <typename F>
    ThisT& add(std::istream& _ris, F _f, const char* _name)
    {
        addStream(_ris, InvalidSize(), _f, _name);
        return *this;
    }

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

    template <typename T>
    ThisT& push(T& _rt, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T&& _rt, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _name);
        return *this;
    }

    ThisT& limits(const Limits& _rlimits)
    {
        SerializerBase::limits(_rlimits);
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
