#include "solid/system/crashhandler.hpp"

namespace solid {
void install_signal_handler()
{
}

void restore_signal_handler_to_default()
{
}

std::string signal_to_string(int signal_number)
{
    return "";
}

// restore to whatever signal handler was used before signal handler installation
void restore_signal_handler(int signal_number)
{
}

namespace internal {
bool should_block_for_fatal_handling()
{
    return false;
}

/** \return signal_name Ref: signum.hpp and \ref installSignalHandler
 *  or for Windows exception name */
std::string exit_reason_name(const char* _text, SignalType signal_number)
{
    return "";
}

/** return calling thread's stackdump*/
std::string stackdump(const char* dump /* = nullptr*/)
{
    return "";
}
} // namespace internal

} // namespace solid