#pragma once

#include "solid/serialization/v2/binarybase.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/innerlist.hpp"
#include <deque>
#include <istream>
#include <ostream>
#include <vector>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

class DeserializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(DeserializerBase&, Runnable&, void*);

    using FunctionT = SOLID_FUNCTION(ReturnE(DeserializerBase&, Runnable&, void*));

    struct Runnable : inner::Node<InnerListCount> {
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
        {
        }

        template <class F>
        Runnable(void* _ptr, CallbackT _call, F _f, const char* _name)
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
            void*       _ptr,
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

        void*       ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
    };

public:
    static constexpr bool is_serializer   = false;
    static constexpr bool is_deserializer = true;

public:
    DeserializerBase();

    std::istream& run(std::istream& _ris, void* _pctx = nullptr);
    long          run(const char* _pbeg, unsigned _sz, void* _pctx = nullptr);
    
    void clear();
    
    void limits(const Limits &_rlimits);
    
    const Limits& limits()const{
        return Base::limits();
    }
    const ErrorConditionT& error()const{
        return Base::error();
    }

    void addBasic(bool& _rb, const char* _name);
    void addBasic(int8_t& _rb, const char* _name);
    void addBasic(uint8_t& _rb, const char* _name);
    void addBasic(std::string& _rb, const char* _name);

    template <class T>
    void addBasic(T& _rb, const char* _name)
    {
        idbg(_name);
#if 0
        Runnable r{&_rb, &load_binary, sizeof(T), 0, _name};

        if (isRunEmpty()) {
            if (load_binary(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }
#else
        Runnable r{&_rb, &load_cross_with_check<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (load_cross_with_check<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }
#endif
        schedule(std::move(r));
    }
    
    template <class T>
    void addBasicWithCheck(T& _rb, const char* _name)
    {
        idbg(_name);

        Runnable r{&_rb, &load_cross_with_check<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (load_cross_with_check<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class D, class F>
    void addFunction(D& _rd, F _f, const char* _name)
    {
        idbg(_name);
        if (isRunEmpty() and _rd.pcrt_ != _rd.pend_) {
            _f(_rd, _name);
        } else {
            Runnable r{
                call_function,
                [_f](DeserializerBase& _rd, Runnable& _rr, void* _pctx) {
                    size_t old_sentinel = _rd.sentinel();

                    _f(static_cast<D&>(_rd), _rr.name_);

                    const bool is_run_empty = _rd.isRunEmpty();
                    _rd.sentinel(old_sentinel);
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

    template <class D, class F, class Ctx>
    void addFunction(D& _rd, F _f, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        if (isRunEmpty() and _rd.pcrt_ != _rd.pend_) {
            _f(_rd, _rctx, _name);
        } else {
            Runnable r{
                nullptr,
                call_function,
                [_f](DeserializerBase& _rd, Runnable& _rr, void* _pctx) {
                    size_t old_sentinel = _rd.sentinel();

                    _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);

                    const bool is_run_empty = _rd.isRunEmpty();
                    _rd.sentinel(old_sentinel);
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

    template <class D, class F>
    void pushFunction(D& _rd, F&& _f, const char* _name)
    {
        idbg(_name);
        auto lambda = [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
        {
            size_t     old_sentinel = _rd.sentinel();
            const bool done         = _f(static_cast<D&>(_rd), _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty();
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
    void pushFunction(D& _rd, F&& _f, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        auto lambda = [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
        {
            size_t     old_sentinel = _rd.sentinel();
            const bool done         = _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty();
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
    void addContainer(D& _rd, C& _rc, const char* _name)
    {
        idbg(_name);
        
        typename C::value_type value;
        bool                   init          = true;
        bool                   parsing_value = false;
        auto                   lambda        = [value, parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            C&   rcontainer = *static_cast<C*>(_rr.ptr_);
            D&   rd         = static_cast<D&>(_rd);

            if (init) {
                init                = false;
                size_t old_sentinel = _rd.sentinel();
                SOLID_ASSERT(_rd.isRunEmpty());
                
                rd.addBasicWithCheck(_rr.size_, _rr.name_);
                
                const bool is_run_empty = _rd.isRunEmpty();
                _rd.sentinel(old_sentinel);
                if (not is_run_empty) {
                    return ReturnE::Wait;
                }
            }

            if (parsing_value) {
                rcontainer.insert(rcontainer.end(), value);
                parsing_value = false;
            } else if(_rd.limits().hasContainer() && _rr.size_ > _rd.limits().container()){
                _rd.error(error_limit_stream);
                return ReturnE::Done;
            }

            size_t old_sentinel = _rd.sentinel();

            while (_rd.pcrt_ != _rd.pend_ and _rr.size_ != 0) {
                rd.add(value, _rr.name_);
                --_rr.size_;

                if (_rd.isRunEmpty()) {
                    //the value was parsed
                    rcontainer.insert(rcontainer.end(), value);
                } else {
                    parsing_value = true;
                    SOLID_CHECK(_rd.pcrt_ == _rd.pend_, "buffer not empty");
                }
            }

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);

            if (_rr.size_ == 0 and is_run_empty) {
                return ReturnE::Done;
            }
            return ReturnE::Wait;
        };

        Runnable r{&_rc, call_function, lambda, _name};

        tryRun(std::move(r));
    }

    template <class D, class C, class Ctx>
    void addContainer(D& _rd, C& _rc, Ctx& _rctx, const char* _name)
    {
        idbg(_name);

        typename C::value_type value;
        bool                   init          = true;
        bool                   parsing_value = false;
        auto                   lambda        = [value, parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
            C&   rcontainer = *static_cast<C*>(_rr.ptr_);
            D&   rd         = static_cast<D&>(_rd);
            Ctx& rctx       = *static_cast<Ctx*>(_pctx);

            if (init) {
                init                = false;
                size_t old_sentinel = _rd.sentinel();
                SOLID_ASSERT(_rd.isRunEmpty());
                
                rd.addBasicWithCheck(_rr.size_, _rr.name_);
                
                const bool is_run_empty = _rd.isRunEmpty();
                _rd.sentinel(old_sentinel);
                if (not is_run_empty) {
                    return ReturnE::Wait;
                }
            }

            if (parsing_value) {
                rcontainer.insert(rcontainer.end(), value);
                parsing_value = false;
            } else if(_rd.limits().hasContainer() && _rr.size_ > _rd.limits().container()){
                _rd.error(error_limit_stream);
                return ReturnE::Done;
            }

            size_t old_sentinel = _rd.sentinel();

            while (_rd.pcrt_ != _rd.pend_ and _rr.size_ != 0) {
                rd.add(value, rctx, _rr.name_);
                --_rr.size_;

                if (_rd.isRunEmpty()) {
                    //the value was parsed
                    rcontainer.insert(rcontainer.end(), value);
                } else {
                    parsing_value = true;
                    SOLID_CHECK(_rd.pcrt_ == _rd.pend_, "buffer not empty");
                }
            }

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);

            if (_rr.size_ == 0 and is_run_empty) {
                return ReturnE::Done;
            }
            return ReturnE::Wait;
        };

        Runnable r{&_rc, call_function, lambda, _name};

        tryRun(std::move(r), &_rctx);
    }

    template <class S, class T>
    void addPointer(S& _rd, std::shared_ptr<T>& _rp, const char* _name)
    {
        idbg(_name);
    }

    template <class S, class T, class Ctx>
    void addPointer(S& _rd, std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
    }

    template <class S, class T, class D>
    void addPointer(S& _rd, std::unique_ptr<T, D>& _rp, const char* _name)
    {
        idbg(_name);
    }

    template <class S, class T, class D, class Ctx>
    void addPointer(S& _rd, std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
    }

    template <class F, class Ctx>
    void addStream(std::ostream& _ros, F _f, Ctx& _rctx, const char* _name)
    {
        uint64_t len    = 0;
        auto     lambda = [ _f = std::move(_f), len ](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
        {
            Ctx&          rctx = *static_cast<Ctx*>(_pctx);
            std::ostream& ros  = *const_cast<std::ostream*>(static_cast<const std::ostream*>(_rr.ptr_));
            len += _rr.data_;
            
            _f(ros, len, _rr.data_ == 0, rctx, _rr.name_);
            
            if(_rd.limits().hasStream() && len > _rd.limits().stream()){
                _rd.error(error_limit_stream);
            }
            return ReturnE::Done;
        };

        Runnable r{&_ros, &load_stream, 0, 0, lambda, _name};
        if (isRunEmpty()) {
            if (load_stream(*this, r, &_rctx) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

private:
    void tryRun(Runnable&& _ur, void* _pctx = nullptr);

    size_t schedule(Runnable&& _ur);
    
    void error(const ErrorConditionT &_err){
        if(!error_){
            error_ = _err;
            pcrt_ = pbeg_ = pend_ = nullptr;
        }
    }

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

    static ReturnE load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_byte(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    
    template <typename T>
    static ReturnE load_cross(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
    {
        return ReturnE::Wait;
    }
    
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

                    T vt = static_cast<T>(v);

                    if(static_cast<uint64_t>(vt) == v){
                        *reinterpret_cast<T*>(_rr.ptr_) = vt;
                    }else{
                        _rd.error(error_cross_integer);
                    }
                    
                    return ReturnE::Done;
                } else {
                    //not enough data
                    _rr.size_ = cross::size(_rd.pcrt_);
                    
                    if(_rr.size_ == InvalidSize()){
                        _rd.error(error_cross_integer);
                        return ReturnE::Done;
                    }
                    
                    size_t toread = _rd.pend_ - _rd.pcrt_;
                    SOLID_CHECK(toread < _rr.size_, "Should not happen");
                    memcpy(_rd.buf_ + _rr.data_, _rd.pcrt_, toread);
                    _rd.pcrt_ += toread;
                    _rr.size_ -= toread;
                    _rr.data_ += toread;
                    return ReturnE::Wait;
                }
            } else {
                size_t toread = _rd.pend_ - _rd.pcrt_;
                SOLID_CHECK(toread >= _rr.size_, "Should not happen");
                if (toread > _rr.size_) {
                    toread = _rr.size_;
                }
                memcpy(_rd.buf_ + _rr.data_, _rd.pcrt_, toread);
                _rd.pcrt_ += toread;
                _rr.size_ -= toread;
                _rr.data_ += toread;
                if (_rr.size_ == 0) {
                    uint64_t    v;
                    T           vt;
                    const char* p = cross::load_with_check(_rd.buf_, _rr.data_, v);
                    if(p == nullptr){
                        _rd.error(error_cross_integer);
                        return ReturnE::Done;
                    }

                    vt = static_cast<T>(v);

                    if(static_cast<uint64_t>(vt) == v){
                        *reinterpret_cast<T*>(_rr.ptr_) = vt;
                    }else{
                        _rd.error(error_cross_integer);
                    }

                    return ReturnE::Done;
                }
                return ReturnE::Wait;
            }
        }
        return ReturnE::Wait;
    }

    static ReturnE noop(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

    static ReturnE load_stream(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk_begin(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_stream_chunk(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

private:
    enum {
        BufferCapacityE = sizeof(uint64_t) * 2
    };

    using RunVectorT = std::deque<Runnable>;
    using RunListT   = inner::List<RunVectorT, InnerListRun>;
    using CacheListT = inner::List<RunVectorT, InnerListCache>;

    const char* pbeg_;
    const char* pend_;
    const char* pcrt_;
    size_t      sentinel_;
    RunVectorT  run_vec_;
    RunListT    run_lst_;
    CacheListT  cache_lst_;
    char        buf_[BufferCapacityE];
}; // namespace v2

template <class Ctx = void>
class Deserializer;


template <>
class Deserializer<void> : public DeserializerBase {
public:
    using ThisT = Deserializer<void>;

    template <typename F>
    ThisT& add(std::ostream& _ros, F _f, const char* _name)
    {
        addStream(_ros, _f, _name);
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
    
    ThisT& limits(const Limits &_rlimits){
        DeserializerBase::limits(_rlimits);
        return *this;
    }
};


template <class Ctx>
class Deserializer : public DeserializerBase {
public:
    using ThisT    = Deserializer<Ctx>;
    using ContextT = Ctx;

    template <typename F>
    ThisT& add(std::ostream& _ros, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ros, _f, _rctx, _name);
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
    
    ThisT& limits(const Limits &_rlimits){
        DeserializerBase::limits(_rlimits);
        return *this;
    }
    
    std::istream& run(std::istream& _ris, Ctx& _rctx)
    {
        return DeserializerBase::run(_ris, &_rctx);
    }

    std::pair<ThisT&, Ctx&> wrap(Ctx& _rct)
    {
        return std::make_pair(std::ref(*this), std::ref(_rct));
    }
};

template <typename D>
inline std::istream& operator>>(std::istream& _ris, std::pair<D&, typename D::ContextT&> _des)
{
    return _des.first.run(_ris, _des.second);
}

} // namespace binary
} // namespace v2
} // namespace serialization
} // namespace solid
