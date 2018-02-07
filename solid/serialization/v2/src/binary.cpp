
#include "solid/serialization/v2/binaryserializer.hpp"
#include "solid/serialization/v2/binarydeserializer.hpp"

namespace solid{
namespace serialization{
namespace v2{
namespace binary{

//== Serializer  ==============================================================

void SerializerBase::addBasic(const bool &_rb, const char *_name){
    idbg("");
    
}

void SerializerBase::addBasic(const int32_t &_rb, const char *_name){
    idbg("");
}
void SerializerBase::addBasic(const uint64_t &_rb, const char *_name){
    idbg("");
}

//== Deserializer  ============================================================

void DeserializerBase::addBasic(bool &_rb, const char *_name){
    idbg("");
}

void DeserializerBase::addBasic(int32_t &_rb, const char *_name){
    idbg("");
}
void DeserializerBase::addBasic(uint64_t &_rb, const char *_name){
    idbg("");
}

}//namespace binary
}//namespace v2
}//namespace serialization
}//namespace solid
