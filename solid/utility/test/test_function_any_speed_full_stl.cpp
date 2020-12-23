#define SOLID_USE_STD_FUNCTION
#define USE_STD_QUEUE
#define SOLID_EVENT_USE_STD_ANY

#include "solid/system/log.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/function_new.hpp"
#include "solid/utility/queue.hpp"
#include <queue>
#include <iostream>
#include "test_function_any_speed.hpp"

namespace full_stl{
#include "test_function_any_speed.cpp"
}
int test_function_any_speed_full_stl(int argc, char * argv[]){
    return full_stl::test_function_any_speed(argc, argv);
}