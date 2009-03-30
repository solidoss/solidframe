#ifndef TEST_DYN_COMMANDS_HPP
#define TEST_DYN_COMMANDS_HPP

#include "testcommand.hpp"

namespace test_cmds{

struct FirstCommand: public test_base::BaseCommand{
	DYNAMIC_DECLARATION;
	
	FirstCommand(uint32 _v);
	uint32 v;
};

struct SecondCommand: public test_base::BaseCommand{
	DYNAMIC_DECLARATION;
	
	SecondCommand(const char *_v);
	const char *v;
};

struct ThirdCommand: SecondCommand{
	DYNAMIC_DECLARATION;
	
	ThirdCommand(char _v1, const char *_v2, uint32 _v3 = 0);
	char v;
};

}

#endif
