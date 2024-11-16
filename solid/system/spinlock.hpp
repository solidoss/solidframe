// solid/system/spinlock.hpp
//
// Copyright (c) 2022 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include "solid/system/common.hpp"

#ifdef SOLID_USE_PTHREAD_SPINLOCK

#include <pthread.h>

#else
#include <atomic>
#include <mutex>

#if defined(_MSC_VER)
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define _WINSOCKAPI_
#include <windows.h>
#elif defined(__i386__) || defined(__x86_64__)
#if defined(__clang__)
#include <emmintrin.h>
#endif
#endif
#endif

namespace solid {

inline void cpu_pause()
{
#if defined(_MSC_VER)
    YieldProcessor();
#elif defined(__i386__) || defined(__x86_64__)
#if defined(__clang__)
    _mm_pause();
#else
    __builtin_ia32_pause();
#endif
#elif defined(__arm__)
    __asm__ volatile("yield" ::
            : "memory");
#else
#endif
}

#if defined(SOLID_USE_PTHREAD_SPINLOCK)
class SpinLock : NonCopyable {
    pthread_spinlock_t spin_;

public:
    SpinLock()
    {
        solid_check(pthread_spin_init(&spin_, PTHREAD_PROCESS_PRIVATE) == 0);
    }
    void lock() noexcept
    {
        pthread_spin_lock(&spin_);
    }

    bool try_lock() noexcept
    {
        return pthread_spin_trylock(&spin_) == 0;
    }

    void unlock() noexcept
    {
        pthread_spin_unlock(&spin_);
    }
};
#else

#if defined(__cpp_lib_atomic_flag_test)
class SpinLock : NonCopyable {
    std::atomic_flag atomic_flag = ATOMIC_FLAG_INIT;

public:
    void lock() noexcept
    {
        for (;;) {
            if (!atomic_flag.test_and_set(std::memory_order_acquire)) {
                break;
            }
            while (atomic_flag.test(std::memory_order_relaxed)) {
                cpu_pause();
            }
        }
    }

    bool try_lock() noexcept
    {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !atomic_flag.test(std::memory_order_relaxed) && !atomic_flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        atomic_flag.clear(std::memory_order_release);
    }
};
#else
// https://rigtorp.se/spinlock/
class SpinLock : NonCopyable {
    std::atomic<bool> lock_ = {0};

public:
    void lock() noexcept
    {
        for (;;) {
            // Optimistically assume the lock is free on the first try
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                return;
            }
            // Wait for lock to be released without generating cache misses
            while (lock_.load(std::memory_order_relaxed)) {
                // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
                // hyper-threads
                cpu_pause();
            }
        }
    }

    bool try_lock() noexcept
    {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !lock_.load(std::memory_order_relaxed) && !lock_.exchange(true, std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        lock_.store(false, std::memory_order_release);
    }
};
#endif
#endif

using SpinGuardT = std::lock_guard<SpinLock>;

} // namespace solid