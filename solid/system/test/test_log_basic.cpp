#include "solid/system/log.hpp"
#include <iostream>
#include <sstream>

using namespace std;

namespace {
solid::LoggerT logger{"test"};
}

int test_log_basic(int argc, char* argv[])
{

    ostringstream oss;

    solid::log_start(oss, "view", {"*:view"});

    solid_log(solid_logger, Info, "First line of log: " << argc << " " << argv[0]);

    string s{oss.str()};
    cout.write(s.data(), s.size());
    return 0;
}
