#include "solid/system/log.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/any.hpp"
#include <queue>
#include <iostream>
#include "test_function_any_speed.hpp"

namespace full_solid{
#include "test_function_any_speed.cpp"
}
int test_function_any_speed_full_solid(int argc, char * argv[]){
#ifdef SOLID_USE_STD_FUNCTION
    static_assert(false);
#endif
    return full_solid::test_function_any_speed(argc, argv);
}
