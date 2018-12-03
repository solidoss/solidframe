#include "solid/system/exception.hpp"
#include "solid/system/crashhandler.hpp"

using namespace solid;
using namespace std;


int test_crashhandler(int argc, char *argv[]){
    install_crash_handler();
    
    solid_check(false);
    return 0;
}
