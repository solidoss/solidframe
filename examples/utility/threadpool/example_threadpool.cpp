// example_threadpool.cpp
//
// Copyright (c) 2007, 2008, 2018, 2025 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/log.hpp"
#include "solid/utility/threadpool.hpp"
#include <cassert>
#include <iostream>
#include <mutex>
#include <string>
#include <variant>
using namespace std;
using namespace solid;

namespace {
Logger logger{"test"};
} // namespace

struct First {
    uint64_t v1_;

    First(uint64_t _v)
        : v1_(_v)
    {
    }
};

struct Second {
    uint64_t v1_;
    uint64_t v2_;

    Second(uint64_t _v)
        : v1_(_v)
        , v2_(_v)
    {
    }
};

struct Third {
    uint64_t v1_;
    uint64_t v2_;
    uint64_t v3_;

    Third(uint64_t _v)
        : v1_(_v)
        , v2_(_v)
        , v3_(_v)
    {
    }
};

using VariantT = std::variant<string, First, Second, Third, unique_ptr<Third>>;

using ThreadPoolT = ThreadPool<VariantT, size_t>;

struct Context {
    mutex mtx_;
};

void handle(First& _rv, Context& _rctx)
{
    lock_guard<mutex> lock(_rctx.mtx_);
    solid_log(logger, Info, "Handle First");
}

void handle(Second& _rv, Context& _rctx)
{
    lock_guard<mutex> lock(_rctx.mtx_);
    solid_log(logger, Info, "Handle Second");
}

void handle(Third& _rv, Context& _rctx)
{
    lock_guard<mutex> lock(_rctx.mtx_);
    solid_log(logger, Info, "Handle Third");
}

void handle(string& _rv, Context& _rctx)
{
    lock_guard<mutex> lock(_rctx.mtx_);
    solid_log(logger, Info, "Handle string " << _rv);
}

void handle(unique_ptr<Third>& _rv, Context& _rctx)
{
    lock_guard<mutex> lock(_rctx.mtx_);
    solid_log(logger, Info, "Handle unique_ptr<Third>");
}

int main(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EW", "test:VIEW"});

    Context ctx;

    ThreadPoolT tp{
        {4},
        [](size_t, Context&) {},
        [](size_t, Context&) {},
        [](VariantT& _var, Context& _rctx) {
            std::visit([&_rctx](auto& _rv) { handle(_rv, _rctx); }, _var);
        },
        [](size_t, Context&) {

        },
        ref(ctx)};

    for (size_t i = 0; i < 100; ++i) {
        switch (i % variant_size_v<VariantT>) {
        case 0:
            tp.pushOne(to_string(i));
            break;
        case 1:
            tp.pushOne(First(i));
            break;
        case 2:
            tp.pushOne(Second(i));
            break;
        case 3:
            tp.pushOne(Third(i));
            break;
        case 4:
            tp.pushOne(make_unique<Third>(i));
            break;
        default:
            assert(false);
        }
    }

    tp.stop();
    solid_log(logger, Statistic, "ThreadPool statistic: " << tp.statistic());
}
