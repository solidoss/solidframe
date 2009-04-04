#include "testcommands.hpp"

namespace test_base{

DYNAMIC_DEFINITION(BaseCommand);

BaseCommand::BaseCommand(){
}

}


namespace test_cmds{

DYNAMIC_DEFINITION(FirstCommand);

FirstCommand::FirstCommand(uint32 _v):v(_v){
}

DYNAMIC_DEFINITION(SecondCommand);

SecondCommand::SecondCommand(const char *_v):v(_v){
}

DYNAMIC_DEFINITION(ThirdCommand);

ThirdCommand::ThirdCommand(char _v1, const char *_v2, uint32 _v3):SecondCommand(_v2),v(_v1){
}


}
