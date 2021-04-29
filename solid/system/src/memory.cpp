// solid/system/src/memorycache.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/memory.hpp"
#include <cstdlib>

#ifdef SOLID_ON_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

#ifdef SOLID_ON_LINUX
#include <malloc.h>
#endif

namespace {
size_t getMemorySize();
size_t getMemoryPageSize();
} //namespace

namespace solid {

size_t memory_page_size()
{
    return getMemoryPageSize();
}

void* memory_allocate_aligned(size_t _align, size_t _size)
{
#ifdef SOLID_ON_WINDOWS
    return nullptr;
#elif defined(SOLID_ON_DARWIN) || defined(SOLID_ON_FREEBSD)
    void* retval = 0;
    int   rv     = posix_memalign(&retval, _align, _size);
    if (rv == 0) {
        return retval;
    } else {
        return nullptr;
    }
#else
    return memalign(_align, _size);
#endif
}
void memory_free_aligned(void* _pv)
{
#ifdef SOLID_ON_WINDOWS
#else
    free(_pv);
#endif
}

size_t memory_size()
{
    return getMemorySize();
}

} //namespace solid

namespace {
/**
 * Returns the size of physical memory (RAM) in bytes.
 */
size_t getMemoryPageSize()
{
#ifdef SOLID_ON_WINDOWS
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwPageSize;
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

#if defined(_WIN32)

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(BSD)
#include <sys/sysctl.h>
#endif

#else
#error "Unable to define getMemorySize( ) for an unknown OS."
#endif

/**
 * Returns the size of physical memory (RAM) in bytes.
 */
size_t getMemorySize()
{
#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
    /* Cygwin under Windows. ------------------------------------ */
    /* New 64-bit MEMORYSTATUSEX isn't available.  Use old 32.bit */
    MEMORYSTATUS status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatus(&status);
    return (size_t)status.dwTotalPhys;

#elif defined(_WIN32)
    /* Windows. ------------------------------------------------- */
    /* Use new 64-bit MEMORYSTATUSEX, not old 32-bit MEMORYSTATUS */
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t)status.ullTotalPhys;

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    /* UNIX variants. ------------------------------------------- */
    /* Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM */

#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
    int mib[2];
    mib[0]       = CTL_HW;
#if defined(HW_MEMSIZE)
    mib[1]       = HW_MEMSIZE; /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
    mib[1] = HW_PHYSMEM64; /* NetBSD, OpenBSD. --------- */
#endif
    int64_t size = 0; /* 64-bit */
    size_t  len  = sizeof(size);
    if (sysctl(mib, 2, &size, &len, nullptr, 0) == 0)
        return (size_t)size;
    return 0L; /* Failed? */

#elif defined(_SC_AIX_REALMEM)
    /* AIX. ----------------------------------------------------- */
    return (size_t)sysconf(_SC_AIX_REALMEM) * (size_t)1024L;

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    /* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
    return static_cast<size_t>(sysconf(_SC_PHYS_PAGES)) * static_cast<size_t>(sysconf(_SC_PAGESIZE));

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
    /* Legacy. -------------------------------------------------- */
    return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGE_SIZE);

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
    /* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
    int mib[2];
    mib[0]            = CTL_HW;
#if defined(HW_REALMEM)
    mib[1]            = HW_REALMEM; /* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
    mib[1] = HW_PHYSMEM; /* Others. ------------------ */
#endif
    unsigned int size = 0; /* 32-bit */
    size_t       len  = sizeof(size);
    if (sysctl(mib, 2, &size, &len, nullptr, 0) == 0)
        return (size_t)size;
    return 0L; /* Failed? */
#endif /* sysctl and sysconf variants */

#else
    return 0L; /* Unknown OS. */
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

} //namespace
