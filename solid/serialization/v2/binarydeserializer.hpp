#pragma once

#include "solid/serialization/v2/binarybase.hpp"

namespace solid{
namespace serialization{
namespace v2{

class DeserializerBase: public Base{
    
};

template <class Ctx = void>
class Deserializer;

template <class Ctx>
class Deserializer: DeserializerBase{
public:
    using ThisT = Serializer<Ctx>;
    
    template <typename T>
    ThisT& add(const T &_rt, Ctx &_rctx, const char *_name){
        
        return *this;
    }
};

}//namespace v2
}//namespace serialization
}//namespace solid

