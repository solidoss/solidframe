#include "solid/system/log.hpp"
#include <iostream>
#include <sstream>

using namespace std;

namespace {
solid::LoggerT logger{"test"};
}

int test_log_file(int argc, char* argv[])
{

    ostringstream oss;

    solid::log_start(oss, {".*:VIEW"});

    solid_log(solid::generic_logger, Info, "First line of log: " << argc << " " << argv[0]);
    solid_log(logger, Verbose, "Second line of log: " << argc << ' ' << argv[0]);

    string s{oss.str()};
    cout.write(s.data(), s.size());
    return 0;
}
