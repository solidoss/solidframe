#pragma once

#include "solid/serialization/v2/binarybase.hpp"
#include <ostream>

namespace solid{
namespace serialization{
namespace v2{
namespace binary{
class SerializerBase: public Base{
    
};

template <class Ctx = void>
class Serializer;

template <>
class Serializer<void>: SerializerBase{
public:
    using ThisT = Serializer<void>;
    
    template <typename T>
    ThisT& add(const T &_rt, const char *_name){
        
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
