#include "solid/utility/ioformat.hpp"
#include "solid/system/exception.hpp"
#include <iostream>
#include <sstream>

using namespace std;
using namespace solid;

int test_ioformat(int argc, char *argv[]){
    {
        ostringstream oss;
        
        oss<<format<64>("%04u.%03u", 1000, 333)<<'_';
        
        cout<<oss.str()<<endl;
        
        SOLID_CHECK(oss.str() == "1000.333_");
    }
    {
        ostringstream oss;
        
        oss<<format<0>("%04u.%03u", 1000, 333)<<'_';
        
        cout<<oss.str()<<endl;
        
        SOLID_CHECK(oss.str() == "1000.333_");
    }
    return 0;
}


