#pragma once
#include <functional>
#include "solid/serialization/v2/binarybase.hpp"
#include <istream>

namespace solid{
namespace serialization{
namespace v2{
namespace binary{


#define SOLID_SERIALIZATION_V2_BASIC_DECL(tp)                       \
    template <class S>                       \
    void solidSerializeV2(S& _s, tp& _t)       \
    {                                        \
        _s.push(_t, "basic");                \
    }                                        \
    template <class S, class Ctx>            \
    void solidSerializeV2(S& _s, tp& _t, Ctx&) \
    {                                        \
        _s.push(_t, "basic");                \
    }


class DeserializerBase: public Base{
protected:
    templae
};

template <class Ctx = void>
class Deserializer;

template <class Ctx>
class Deserializer: DeserializerBase{
public:
    using ThisT = Deserializer<Ctx>;
    using ContextT = Ctx;
    
    template <typename T>
    ThisT& add(T &_rt, Ctx &_rctx, const char *_name){
        add_ref(_rt, _rctx, _name);
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

