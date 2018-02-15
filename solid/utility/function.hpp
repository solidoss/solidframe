#pragma once

#include "solid/utility/any.hpp"
#include "solid/utility/common.hpp"


namespace solid{

template<size_t DataSize, class> class Function; // undefined

template<size_t DataSize, class R, class... ArgTypes>
class Function<DataSize, R(ArgTypes...)>{
    
    struct StubBase{
        virtual ~StubBase(){}
        virtual R operator()(ArgTypes... args) = 0;
    };
    
    template <class T>
    struct Stub: StubBase{
        T   t_;
        
        Stub(T &&_ut):t_(std::move(_ut)){}
        
        R operator()(ArgTypes... args) override{
            return t_(std::forward<ArgTypes>(args)...);
        }
    };
public:
    template <class T>
    Function(T &&_ut):any_(Stub<T>(std::move(_ut))){
        
    }
    
    
    explicit operator bool() const noexcept{
        return not any_.empty();
    }
    
    R operator()(ArgTypes... args) const{
        return (*any_.cast<StubBase>())(std::forward<ArgTypes>(args)...);
    }
private:
    Any<DataSize>   any_;
};

}//namespace solid
