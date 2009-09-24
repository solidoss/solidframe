#ifndef TEST_DYN_COMMAND_HPP
#define TEST_DYN_COMMAND_HPP

#include "system/common.hpp"
#include "utility/dynamictype.hpp"

namespace test_base{

struct BaseCommand: Dynamic<BaseCommand>{
	BaseCommand();
};

}


#endif
