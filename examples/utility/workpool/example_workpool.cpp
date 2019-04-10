// example_workpool.cpp
//
// Copyright (c) 2007, 2008, 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/log.hpp"
#include "solid/utility/workpool.hpp"
#include <iostream>
#include <thread>

using namespace std;
using namespace solid;

int main(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    WorkPool<int> wp{
        WorkPoolConfiguration(),0,
        [](int _v) {
            solid_log(generic_logger, Info, "v = " << _v);
            std::this_thread::sleep_for(std::chrono::milliseconds(_v * 10));
        }};

    for (int i(0); i < 100; ++i) {
        wp.push(i);
    }
    return 0;
}
