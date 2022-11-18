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

#include <atomic>

namespace solid {

class SpinLock {
    std::atomic_flag atomic_flag = ATOMIC_FLAG_INIT;

public:
    void lock()
    {
        for (;;) {
            if (!atomic_flag.test_and_set(std::memory_order_acquire)) {
                break;
            }
            while (atomic_flag.test(std::memory_order_relaxed))
                ;
        }
    }
    void unlock()
    {
        atomic_flag.clear(std::memory_order_release);
    }
};

} // namespace solid