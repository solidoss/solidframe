#pragma once

#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/binarybase.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/innerlist.hpp"
#include <functional>
#include <istream>
#include <vector>
#include <deque>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

class DeserializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(DeserializerBase&, Runnable&, void*);
    
    using FunctionT = std::function<ReturnE(DeserializerBase&, Runnable &, void*)>;

    struct Runnable : inner::Node<InnerListCount> {
        Runnable(
            void* _ptr,
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
        Runnable(CallbackT _call, F _f, const char *_name):ptr_(nullptr), call_(_call), size_(0), data_(0), name_(_name), fnc_(_f){}

        void        *ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
    };
    
public:
    DeserializerBase();

    std::istream& run(std::istream& _ris, void* _pctx = nullptr);
    long          run(const char* _pbeg, unsigned _sz, void* _pctx = nullptr);
    
    void addBasic(bool& _rb, const char* _name);

    template <class T, class Ctx>
    void addBasic(T& _rb, Ctx& _rctx, const char* _name)
    {
        idbg("");
        Runnable r{&_rb, &load_cross<T>, 0, 0, _name};
        if (run_lst_.empty()) {
            if (load_cross<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class D, class F>
    void addFunction(D& _rd, F _f, const char* _name)
    {
        idbg("");
        if(_rd.pcrt_ != _rd.pend_){
            _f(_rd, _name);
        }else{
            Runnable r{
                load_function,
                [_f](DeserializerBase& _rd, Runnable& _rr, void* _pctx){
                    _f(static_cast<D&>(_rd), _rr.name_);
                    return ReturnE::Done;
                },
                _name
            };
            schedule(std::move(r));
        }
    }

    template <class D, class F, class Ctx>
    void addFunction(D& _rd, F _f, Ctx& _rctx, const char* _name)
    {
        idbg("");
        if(_rd.pcrt_ != _rd.pend_){
            _f(_rd, _rctx, _name);
        }else{
            Runnable r{
                load_function,
                [_f](DeserializerBase& _rd, Runnable& _rr, void* _pctx){
                    _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);
                    return ReturnE::Done;
                },
                _name
            };
            schedule(std::move(r));
        }
    }

    template <class D, class C>
    void addContainer(D& _rd, C& _rc, const char* _name)
    {
        idbg("");
    }

    template <class D, class C, class Ctx>
    void addContainer(D& _rd, C& _rc, Ctx& _rctx, const char* _name)
    {
        idbg("");
        addBasic(data_.u64_, _rctx, _name);
        
        if(_rd.pcrt_ != _rd.pend_){
            if(_data_.u64_ == 0){
                return;
            }
        }
        
        typename C::value_type value;
        auto lambda = [value](DeserializerBase& _rs, Runnable& _rr, void* _pctx)mutable{
            C &rcontainer = *static_cast<const C*>(_rr.ptr_);
            S &rs = static_cast<S&>(_rs);
            
            _rr.data_ = _rs.sentinel();
            
            while(_rs.pcrt_ != _rs.pend_ and it != rcontainer.cend()){
                rs.add(*it, _rr.name_);
                ++it;
            }
            
            if(it != rcontainer.cend()){
                return ReturnE::Wait;
            }else{
                _rs.sentinel(_rr.data_);
                return ReturnE::Done;
            }
        };
    
        Runnable r{&_rc, store_function, 0, 0, _name};
        r.fnc_ = lambda;
        schedule(std::move(r));
        
    }

    template <class S, class T>
    void addPointer(S& _rd, std::shared_ptr<T>& _rp, const char* _name)
    {
        idbg("");
    }

    template <class S, class T, class Ctx>
    void addPointer(S& _rd, std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg("");
    }

    template <class S, class T, class D>
    void addPointer(S& _rd, std::unique_ptr<T, D>& _rp, const char* _name)
    {
        idbg("");
    }

    template <class S, class T, class D, class Ctx>
    void addPointer(S& _rd, std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg("");
    }
private:
    void schedule(Runnable&& _ur);
    
    size_t sentinel(){
        size_t olds = sentinel_;
        sentinel_ = run_lst_.frontIndex();
        return olds;
    }
    
    void sentinel(const size_t _s){
        sentinel_ = _s;
    }
    
    static ReturnE load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    static ReturnE load_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
    
    template <typename T>
    static ReturnE load_cross(DeserializerBase& _rd, Runnable& _rr, void* _pctx){
        if (_rd.pcrt_ != _rd.pend_) {
            if(_rr.size_ == 0){
                //first run
                const char *p = cross::load(_rd.pcrt_, _rd.pend_ - _rd.pcrt_, _rr.data_);
                
                if(p){
                    _rd.pcrt_ = p;
                    return ReturnE::Done;
                }else{
                    //not enough data
                    _rr.size_ = cross::size(_rd.pcrt_);
                    SOLID_CHECK(_rr.size_ !=  InvalidSize(), "TODO: handle error");
                    size_t toread = _rd.pend_ - _rd.pcrt_;
                    SOLID_CHECK(toread < _rr.size_, "Should not happen");
                    memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
                    _rd.pcrt_ += toread;
                    _rr.size_ -= toread;
                    _rr.data_ += toread;
                    return ReturnE::Wait;
                }
            }else{
                size_t toread = _rd.pend_ - _rd.pcrt_;
                SOLID_CHECK(toread < _rr.size_, "Should not happen");
                memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
                _rd.pcrt_ += toread;
                _rr.size_ -= toread;
                _rr.data_ += toread;
                if(_rr.size_ == 0){
                    uint64_t v;
                    T        vt;
                    const char *p = cross::load(_rd.data_.buf_, _rr.data_, v);
                    SOLID_CHECK(p != nullptr, "TODO: handle error");
                    
                    vt = static_cast<T>(v);
                    
                    SOLID_CHECK(static_cast<uint64_t>(vt) == v, "TODO: handle error");
                    
                    *reinterpret_cast<T*>(_rr.ptr_) = vt;
                    return ReturnE::Done;
                }
                return ReturnE::Wait;
            }
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
    
    const char*      pbeg_;
    const char*      pend_;
    const char*      pcrt_;
    size_t     sentinel_;
    RunVectorT run_vec_;
    RunListT   run_lst_;
    CacheListT cache_lst_;
    union {
        uint64_t   u64_;
        char       buf_[BufferCapacityE];
    }   data_;
};

template <class Ctx = void>
class Deserializer;

template <class Ctx>
class Deserializer : public DeserializerBase {
public:
    using ThisT    = Deserializer<Ctx>;
    using ContextT = Ctx;

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

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
