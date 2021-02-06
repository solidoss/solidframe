// solid/serialization/v3/serialization.hpp
//
// Copyright (c) 2019 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/serialization/v3/binarybasic.hpp"
#include "solid/reflection/v1/typemap.hpp"

namespace solid {
namespace serialization {
namespace v3{
namespace binary{

template <class MetadataVariant, class MetadataFactory, class Context, typename TypeId>
class Serializer{
public:
    using context_type = Context;
    static constexpr bool is_const_reflector = true;
    
    
    Serializer(MetadataFactory &_metadata_factory, const reflection::v1::TypeMapBase &_rtype_map){}
    
    template <typename F>
    long run(char* _pbeg, unsigned _sz, F _f, Context& _rctx)
    {
        return 0;
    }

    template <typename F>
    std::ostream& run(std::ostream& _ros, F _f, Context& _rctx)
    {
        return _ros;
    }

    long run(char* _pbeg, unsigned _sz, Context& _rctx)
    {
        return 0;
    }
    
    template <typename T, typename F>
    auto& add(const T &_rt, Context &_rctx, const size_t _id, const char *const _name, F _f){
        
        return *this;
    }

    template <typename T>
    auto& add(const T &_rt, Context &_rctx, const size_t _id, const char * const _name){
        return *this;
    }
    
    const ErrorConditionT& error() const
    {
        static const ErrorConditionT err;
        return err;
    }
    void clear(){
        
    }

    bool empty() const
    {
        return true;
    }
};

template <class MetadataVariant, class MetadataFactory, class Context, typename TypeId>
class Deserializer{
public:
    using context_type = Context;
    
    static constexpr bool is_const_reflector = false;
    
    
    Deserializer(MetadataFactory &_metadata_factory, const reflection::v1::TypeMapBase &_rtype_map){}
    
    template <typename T, typename F>
    auto& add(T &_rt, Context &_rctx, const size_t _id, const char *const _name, F _f){
        return *this;
    }

    template <typename T>
    auto& add(T &_rt, Context &_rctx, const size_t _id, const char * const _name){
        return *this;
    }
    
    
    template <typename F>
    long run(const char* _pbeg, unsigned _sz, F _f, Context& _rctx)
    {
        return 0;
    }

    long run(const char* _pbeg, unsigned _sz, Context& _rctx)
    {
        return 0;
    }

    template <typename F>
    std::istream& run(std::istream& _ris, F _f, Context& _rctx)
    {
        return _ris;
    }
    
    const ErrorConditionT& error() const
    {
        static const ErrorConditionT err;
        return err;
    }
    void clear(){
        
    }

    bool empty() const
    {
        return true;
    }
};

}//namespace binary
}//namespace v3

using namespace v3;



} // namespace serialization
} // namespace solid

