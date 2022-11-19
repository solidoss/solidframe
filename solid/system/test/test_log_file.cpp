#include "solid/system/log.hpp"
#include <iostream>
#include <sstream>
#ifdef SOLID_ON_WINDOWS
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace std;

namespace {
solid::LoggerT logger{"test"};
}

int test_log_file(int argc, char* argv[])
{

    const char* prefix = argv[0];

    if (argc > 1) {
        prefix = argv[1];
    }

    int count = 1000 * 1000;

    if (argc > 2) {
        count = atoi(argv[2]);
    }

    auto err = solid::log_start(prefix, {".*:VIEW"}, true, 2, 1024 * 1024 * 10);

    if (err) {
        cout << "Log start error: " << err.message() << endl;
        return 0;
    }

#ifdef SOLID_ON_WINDOWS
    const auto proc_id = GetCurrentProcessId();
#else
    const auto proc_id = getpid();
#endif

    for (int i = 0; i < count; ++i) {
        solid_log(solid::generic_logger, Info, proc_id << ' ' << i << " First line of log: " << argc << " " << argv[0]);
        solid_log(logger, Verbose, proc_id << ' ' << i << " Second line of log: " << argc << ' ' << argv[0]);
    }

    return 0;
}
