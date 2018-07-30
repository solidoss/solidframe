#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include <iostream>
#include <sstream>

using namespace std;

namespace {
solid::LoggerT logger{"test"};
}

int test_log_basic(int argc, char* argv[])
{

    {
        ostringstream oss;

        solid::log_start(oss, {".*:VIEW"});

        solid_log(solid::generic_logger, Info, "First line of log: " << argc << " " << argv[0]);
        solid_log(logger, Verbose, "Second line of log: " << argc << ' ' << argv[0]);

        string s{oss.str()};
        solid_check(!s.empty(), "no log");
        cout.write(s.data(), s.size());
    }

    {
        ostringstream oss;

        solid::log_start(oss, {".*:VI"});

        solid_log(solid::generic_logger, Error, "First line of log: " << argc << " " << argv[0]);
        solid_log(logger, Warning, "Second line of log: " << argc << ' ' << argv[0]);

        string s{oss.str()};
        solid_check(s.empty(), "some log");
        cout.write(s.data(), s.size());
    }

    {
        ostringstream oss;

        solid::log_start(oss, {".*:VIEW", "test:v"});

        solid_log(solid::generic_logger, Info, "First line of log: " << argc << " " << argv[0]);
        solid_log(logger, Verbose, "HIDDEN - Second line of log: " << argc << ' ' << argv[0]);
        solid_log(logger, Info, "Second line of log: " << argc << ' ' << argv[0]);

        string s{oss.str()};
        solid_check(!s.empty(), "no log");
        solid_check(s.find("HIDDEN") == string::npos, "found HIDDEN");
        cout.write(s.data(), s.size());
    }
    return 0;
}
