#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include <iostream>
#include <sstream>

using namespace std;

int test_exception(int argc, char** argv)
{
    bool        is_ok = false;
    std::string check_str;
    {
        ostringstream oss;
        const int     line = 19;
        oss << '[' << __FILE__ << '(' << line << ")][" << CRT_FUNCTION_NAME << "] argc == 0 check failed: some error: " << argc << " " << argv[0] << " " << argv[1];
        check_str = oss.str();
    }
    try {
        SOLID_CHECK(argc == 0, "some error: " << argc << " " << argv[0] << " " << argv[1]);
    } catch (std::logic_error& _rerr) {
        is_ok = true;
        //cout<<check_str<<endl;
        //cout<<_rerr.what()<<endl;
        SOLID_ASSERT(check_str == _rerr.what());
    }

    SOLID_ASSERT(is_ok);

    return 0;
}
