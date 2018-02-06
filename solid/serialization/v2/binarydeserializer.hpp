#pragma once
#include <functional>
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/serialization/v2/binarybase.hpp"
#include <istream>
#include "solid/system/debug.hpp"

namespace solid{
namespace serialization{
namespace v2{
namespace binary{


class DeserializerBase: public Base{
public:
    
    void addBasic(bool &_rb, const char *_name){
        idbg("");
    }
    
    template <class Ctx>
    void addBasic(bool &_rb, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    void addBasic(int32_t &_rb, const char *_name){
        idbg("");
    }
    void addBasic(uint64_t &_rb, const char *_name){
        idbg("");
    }
    
    template <class Ctx>
    void addBasic(int32_t &_rb, Ctx &_rctx, const char *_name){
        idbg("");
    }
    template <class Ctx>
    void addBasic(uint64_t &_rb, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    
    
    template <class D, class F>
    void addFunction(D &_rd, F &_rf, const char *_name){
        idbg("");
        _rf(_rd, _name);
    }
    
    template <class D, class F, class Ctx>
    void addFunction(D &_rd, F &_rf, Ctx &_rctx, const char *_name){
        idbg("");
        _rf(_rd, _rctx, _name);
    }
    
    template <class D, class C>
    void addContainer(D &_rd, C &_rc, const char *_name){
        idbg("");
    }
    
    template <class D, class C, class Ctx>
    void addContainer(D &_rd, C &_rc, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    template <class S, class T>
    void addPointer(S &_rs, std::shared_ptr<T> &_rp, const char *_name){
        idbg("");
    }
    
    template <class S, class T, class Ctx>
    void addPointer(S &_rs, std::shared_ptr<T> &_rp, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
    template <class S, class T, class D>
    void addPointer(S &_rs, std::unique_ptr<T, D> &_rp, const char *_name){
        idbg("");
    }
    
    template <class S, class T, class D, class Ctx>
    void addPointer(S &_rs, std::unique_ptr<T, D> &_rp, Ctx &_rctx, const char *_name){
        idbg("");
    }
    
};


template <class Ctx = void>
class Deserializer;

template <class Ctx>
class Deserializer: public DeserializerBase{
public:
    using ThisT = Deserializer<Ctx>;
    using ContextT = Ctx;
    
    template <typename T>
    ThisT& add(T &_rt, Ctx &_rctx, const char *_name){
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }
    
    template <typename T>
    ThisT& add(const T &_rt, Ctx &_rctx, const char *_name){
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }
    
    std::istream &run(std::istream &_ris, Ctx &_rctx){
        return _ris;
    }
    
    std::pair<ThisT&, Ctx&> wrap(Ctx &_rct){
        return std::make_pair(std::ref(*this), std::ref(_rct));
    }
    
};

template <typename D>
std::istream& operator>>(std::istream &_ris, std::pair<D&, typename D::ContextT&> _des){
    return _des.first.run(_ris, _des.second);
}

}//namespace binary
}//namespace v2
}//namespace serialization
}//namespace solid

