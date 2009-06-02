#include "testcommands.hpp"

namespace test_base{

BaseCommand::BaseCommand(){
}

}


namespace test_cmds{

FirstCommand::FirstCommand(uint32 _v):v(_v){
}

SecondCommand::SecondCommand(const char *_v):v(_v){
}

ThirdCommand::ThirdCommand(char _v1, const char *_v2, uint32 _v3):BaseTp(_v2),v(_v1){
}


}
