// solid/utility/src/utility.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/mutualstore.hpp"
#include "solid/utility/algorithm.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/string.hpp"
#include "solid/utility/threadpool.hpp"
#include <mutex>
#include <sstream>

namespace solid {

const LoggerT threadpool_logger{"solid::threadpool"};

const uint8_t reverted_chars[] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90,
    0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8,
    0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8,
    0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 0x0C, 0x8C,
    0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2,
    0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A,
    0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, 0x06, 0x86, 0x46, 0xC6,
    0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6,
    0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81,
    0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1,
    0x31, 0xB1, 0x71, 0xF1, 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9,
    0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95,
    0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD,
    0x7D, 0xFD, 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B,
    0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB,
    0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7,
    0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F,
    0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

size_t bit_count(const uint8_t _v)
{
    static const uint8_t cnts[] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};
    return cnts[_v];
}

size_t bit_count(const uint16_t _v)
{
    return bit_count(static_cast<uint8_t>(_v & 0xff)) + bit_count(static_cast<uint8_t>(_v >> 8));
}

size_t bit_count(const uint32_t _v)
{
    return bit_count(static_cast<uint8_t>(_v & 0xff)) + bit_count(static_cast<uint8_t>((_v >> 8) & 0xff)) + bit_count(static_cast<uint8_t>((_v >> 16) & 0xff)) + bit_count(static_cast<uint8_t>((_v >> 24) & 0xff));
}

size_t bit_count(const uint64_t _v)
{
    return bit_count(static_cast<uint32_t>(_v & 0xffffffff)) + bit_count(static_cast<uint32_t>(_v >> 32));
}

uint64_t make_number(std::string _str)
{

    solid_check_log(!_str.empty(), generic_logger, "Empty string");
    uint64_t mul = 1;
    if (isupper(_str.back()) != 0) {
        switch (_str.back()) {
        case 'K':
            mul = 1024ULL;
            break;
        case 'M':
            mul = 1024ULL * 1024;
            break;
        case 'G':
            mul = 1024ULL * 1024 * 1024;
            break;
        case 'T':
            mul = 1024ULL * 1024 * 1024 * 1024;
            break;
        default:
            solid_throw_log(generic_logger, "Unknown multiplier: " << _str.back());
        }
        _str.pop_back();
    }
    if (islower(_str.back()) != 0) {
        switch (_str.back()) {
        case 'k':
            mul = 1000ULL;
            break;
        case 'm':
            mul = 1000ULL * 1000;
            break;
        case 'g':
            mul = 1000ULL * 1000 * 1000;
            break;
        case 't':
            mul = 1000ULL * 1000 * 1000 * 1000;
            break;
        default:
            solid_throw_log(generic_logger, "Unknown multiplier: " << _str.back());
        }
        _str.pop_back();
    }
    const uint64_t n = std::stoull(_str);
    return n * mul;
}

//---------------------------------------------------------------------
//      Shared
//---------------------------------------------------------------------

namespace {

typedef MutualStore<std::mutex> MutexStoreT;

MutexStoreT& mutexStore()
{
    static MutexStoreT mtxstore(true, 3, 2, 2);
    return mtxstore;
}

std::mutex& global_mutex()
{
    static std::mutex mtx;
    return mtx;
}

} // namespace

std::mutex& shared_mutex_safe(const void* _p)
{
    std::lock_guard<std::mutex> lock(global_mutex());
    return mutexStore().safeAt(reinterpret_cast<size_t>(_p));
}
std::mutex& shared_mutex(const void* _p)
{
    return mutexStore().at(reinterpret_cast<size_t>(_p));
}

//-----------------------------------------------------------------------------
// ThreadPool:
ThreadPoolStatistic::ThreadPoolStatistic()
{
    push_one_count_[0] = 0;
    push_one_count_[1] = 0;
}

std::ostream& ThreadPoolStatistic::print(std::ostream& _ros) const
{
    _ros << "create_context_count = " << create_context_count_.load(std::memory_order_relaxed);
    _ros << " delete_context_count = " << delete_context_count_.load(std::memory_order_relaxed);
    _ros << " run_one_free_count = " << run_one_free_count_.load(std::memory_order_relaxed);
    _ros << " max_run_one_free_count = " << max_run_one_free_count_.load(std::memory_order_relaxed);
    _ros << " run_one_context_count = " << run_one_context_count_.load(std::memory_order_relaxed);
    _ros << " max_run_one_context_count = " << max_run_one_context_count_.load(std::memory_order_relaxed);
    _ros << " run_one_push_count = " << run_one_push_count_.load(std::memory_order_relaxed);
    _ros << " max_run_one_push_count = " << max_run_one_push_count_.load(std::memory_order_relaxed);
    _ros << " run_all_wake_count = " << run_all_wake_count_.load(std::memory_order_relaxed);
    _ros << " max_run_all_wake_count = " << max_run_all_wake_count_.load(std::memory_order_relaxed);
    _ros << " push_one_count[free] = " << push_one_count_[0].load(std::memory_order_relaxed);
    _ros << " push_one_count[context] = " << push_one_count_[1].load(std::memory_order_relaxed);
    _ros << " push_all_count = " << push_all_count_.load(std::memory_order_relaxed);
    _ros << " push_all_wake_count = " << push_all_wake_count_.load(std::memory_order_relaxed);
    _ros << " max_consume_all_count = " << max_consume_all_count_.load(std::memory_order_relaxed);
    _ros << " run_all_count = " << run_all_count_.load(std::memory_order_relaxed);
    _ros << " max_run_all_count = " << max_run_all_count_.load(std::memory_order_relaxed);
    _ros << " push_one_wait_lock_count = " << push_one_wait_lock_count_.load(std::memory_order_relaxed);
    _ros << " push_one_wait_pushing_count = " << push_one_wait_pushing_count_.load(std::memory_order_relaxed);
    _ros << " pop_one_wait_lock_count = " << pop_one_wait_lock_count_.load(std::memory_order_relaxed);
    _ros << " pop_one_wait_popping_count = " << pop_one_wait_popping_count_.load(std::memory_order_relaxed);
    _ros << " push_all_wait_lock_count = " << push_all_wait_lock_count_.load(std::memory_order_relaxed);
    _ros << " push_all_wait_pushing_count = " << push_all_wait_pushing_count_.load(std::memory_order_relaxed);
    _ros << " push_one_latency_max_us = " << push_one_latency_max_us_.load(std::memory_order_relaxed);
    _ros << " push_one_latency_min_us = " << push_one_latency_min_us_.load(std::memory_order_relaxed);
    const auto sum_ones = push_one_count_[0].load(std::memory_order_relaxed) + push_one_count_[1].load(std::memory_order_relaxed);
    _ros << " push_one_latency_avg_us = " << sum_ones ? push_one_latency_sum_us_.load(std::memory_order_relaxed) / sum_ones : 0;
    return _ros;
}
void ThreadPoolStatistic::clear() {}
//-----------------------------------------------------------------------------

} // namespace solid
