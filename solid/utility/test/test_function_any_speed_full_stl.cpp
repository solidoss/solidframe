#define SOLID_USE_STD_FUNCTION
#define USE_STD_QUEUE
#define EVENT_USE_STD_ANY

#include "solid/system/log.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/queue.hpp"
#include "test_function_any_speed.hpp"
#include <any>
#include <iostream>
#include <queue>

namespace full_stl {
#include "test_function_any_speed.cpp"
}
int test_function_any_speed_full_stl(int argc, char* argv[])
{
    return full_stl::test_function_any_speed(argc, argv);
}
