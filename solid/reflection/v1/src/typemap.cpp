// solid/reflection/v1/src/typemap.cpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/reflection/v1/typemap.hpp"

namespace solid{
namespace reflection{
namespace v1{

size_t current_index(){
    static std::atomic<size_t> index{0};
    return index.fetch_add(1);
}

}//namespace v1
}//namespace reflection
}//namespace solid
