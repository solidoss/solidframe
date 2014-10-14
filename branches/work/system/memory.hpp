// memory.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_MEMORY_HPP
#define SOLID_SYSTEM_MEMORY_HPP

#include "system/common.hpp"

namespace solid{

size_t memory_page_size();
size_t memory_size();
void * memory_allocate_aligned(size_t _align, size_t _size);
void   memory_free_aligned(void *_pv);

}//namespace solid
#endif