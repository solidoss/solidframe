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

int test_log_socket(int argc, char* argv[])
{

    const char* host = "localhost";
    const char* port = "4444";

    if (argc > 1) {
        host = argv[1];
    }

    if (argc > 2) {
        port = argv[2];
    }

    int count = 1000 * 1000;

    if (argc > 3) {
        count = atoi(argv[2]);
    }

    auto err = solid::log_start(host, port, {".*:VIEW"});
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

    solid::log_stop(); // we must call this when using socket IO to ensure data flush
    return 0;
}
