// solid/utility/src/dynamictype.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/cassert.hpp"
#include <mutex>

#include "solid/utility/common.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/dynamictype.hpp"

#include <vector>

#ifdef SOLID_USE_GNU_ATOMIC
#include <ext/atomicity.h>
#endif

#include <atomic>

namespace solid {

//---------------------------------------------------------------------
//----  DynamicPointer  ----
//---------------------------------------------------------------------

void DynamicPointerBase::clear(DynamicBase* _pdyn)
{
    solid_assert_log(_pdyn, generic_logger);
    if (_pdyn->release() == 0u) {
        delete _pdyn;
    }
}

void DynamicPointerBase::use(DynamicBase* _pdyn)
{
    _pdyn->use();
}

//--------------------------------------------------------------------
//      DynamicBase
//--------------------------------------------------------------------

typedef std::atomic<size_t> AtomicSizeT;

/*static*/ size_t DynamicBase::generateId()
{
    static AtomicSizeT u{0};
    return u.fetch_add(1 /*, std::memory_order_seq_cst*/);
}

/*virtual*/ bool DynamicBase::isTypeDynamic(const size_t /*_id*/)
{
    return false;
}

} // namespace solid
