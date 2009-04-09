#ifndef TEST_DYN_COMMANDS_HPP
#define TEST_DYN_COMMANDS_HPP

#include "testcommand.hpp"

namespace test_cmds{

struct FirstCommand: Dynamic<FirstCommand, test_base::BaseCommand>{
	FirstCommand(uint32 _v);
	uint32 v;
};

struct SecondCommand: Dynamic<SecondCommand, test_base::BaseCommand>{
	SecondCommand(const char *_v);
	const char *v;
};

struct ThirdCommand: Dynamic<ThirdCommand, SecondCommand>{
	ThirdCommand(char _v1, const char *_v2, uint32 _v3 = 0);
	char v;
};

}

#endif
