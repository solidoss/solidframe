// solid/system/src/system.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/exception.hpp"
#include "solid/system/nanotime.hpp"
#include <cstring>

#if defined(SOLID_ON_FREEBSD)
#include <pmc.h>
#elif defined(SOLID_ON_DARWIN)
#elif defined(SOLID_ON_WINDOWS)
#else
#include <sys/sysinfo.h>
#endif

#if defined(SOLID_ON_WINDOWS)
#include <Process.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
//#include <io.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(SOLID_ON_DARWIN)
#include <mach/mach_time.h>
#endif

namespace solid {

/*static*/ const char* src_file_name(char const* _fname)
{
#ifdef SOLID_ON_WINDOWS
    static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system\\src")));
#else
    static const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "solid/system/src")));
#endif
    return _fname + fileoff;
}

//=============================================================================
//  NanoTime
//=============================================================================
/*static*/ const NanoTime NanoTime::maximum{true};

#ifdef SOLID_HAS_NO_INLINES
#include "solid/system/nanotime.ipp"
#endif

} //namespace solid
