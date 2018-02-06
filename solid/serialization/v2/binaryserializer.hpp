#pragma once

#include <ostream>
#include <functional>
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/serialization/v2/binarybase.hpp"
#include "solid/system/debug.hpp"

namespace solid{
namespace serialization{
namespace v2{
namespace binary{

class SerializerBase: public Base{
public:
    
    void addBasic(const bool &_rb, const char *_name){
        idbg("");
    }
    
    void addBasic(const int32_t &_rb, const char *_name){
        idbg("");
    }
    void addBasic(const uint64_t &_rb, const char *_name){
        idbg("");
    }
    
    template <class Ctx>
    void addBasic(const int32_t &_rb, Ctx &_rctx, const char *_name){
        idbg("");
    }
    template <class Ctx>
    void addBasic(const uint64_t &_rb, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    
    
    template <class S, class F>
    void addFunction(S &_rs, F &_rf, const char *_name){
        idbg("");
        _rf(_rs, _name);
    }
    
    template <class S, class F, class Ctx>
    void addFunction(S &_rs, F &_rf, Ctx &_rctx, const char *_name){
        idbg("");
        _rf(_rs, _rctx, _name);
    }
    
    template <class S, class C>
    void addContainer(S &_rs, const C &_rc, const char *_name){
        idbg("");
    }
    
    template <class S, class C, class Ctx>
    void addContainer(S &_rs, const C &_rc, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    template <class S, class T>
    void addPointer(S &_rs, const std::shared_ptr<T> &_rp, const char *_name){
        idbg("");
    }
    
    template <class S, class T, class Ctx>
    void addPointer(S &_rs, const std::shared_ptr<T> &_rp, Ctx &_rctx, const char *_name){
        idbg("");
    }
    template <class S, class T, class D>
    void addPointer(S &_rs, const std::unique_ptr<T, D> &_rp, const char *_name){
        idbg("");
    }
    
    template <class S, class T, class D, class Ctx>
    void addPointer(S &_rs, const std::unique_ptr<T, D> &_rp, Ctx &_rctx, const char *_name){
        idbg("");
    }
};


template <class Ctx = void>
class Serializer;

template <>
class Serializer<void>: public SerializerBase{
public:
    using ThisT = Serializer<void>;
    
    template <typename T>
    ThisT& add(T &_rt, const char *_name){
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }
    
    template <typename T>
    ThisT& add(const T &_rt, const char *_name){
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }
    
    std::ostream& run(std::ostream &_ros){
        return _ros;
    }
};

std::ostream& operator<<(std::ostream &_ros, Serializer<> &_rser){
    return _rser.run(_ros);
}




}//namespace binary
}//namespace v2
}//namespace serialization
}//namespace solid
