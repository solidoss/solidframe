#pragma once

#include <memory>

namespace solid {
namespace serialization {
namespace v2{

struct Base{
    virtual ~Base();
};

class TypeMapBase{
public:
    
    template <class S, typename... Args>
    S createSerializer(Args&&... _args)const{
        return S(*this, std::forward<Args>(_args)...);
    }
    
    template <class D, typename... Args>
    D createDeserializer(Args&&... _args)const{
        return D(*this, std::forward<Args>(_args)...);
    }
    
    template <class T>
    void serialize(Base &_rs, const T *_pt, const char *_name)const{
        
    }
    
    template <class T, class Ctx>
    void serialize(Base &_rs, const T *_pt, Ctx &_rctx, const char *_name)const{
        
    }
    
    template <class T>
    void deserialize(Base &rd, std::shared_ptr<T> &_rsp, const char *_name)const{
        
    }
    template <class T, class Ctx>
    void deserialize(Base &rd, std::shared_ptr<T> &_rsp, Ctx &_rctx, const char *_name)const{
        
    }
    
    template <class T, class D>
    void deserialize(Base &rd, std::unique_ptr<T, D> &_rup, const char *_name)const{
        
    }
    template <class T, class D, class Ctx>
    void deserialize(Base &rd, std::unique_ptr<T, D> &_rup, Ctx &_rctx, const char *_name)const{
        
    }
};

} //namespace v2
} //namespace serialization
} // namespace solid
