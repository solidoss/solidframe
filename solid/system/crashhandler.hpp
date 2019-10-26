#pragma once

/** ==========================================================================
 * Addapted for SolidFrame from https://github.com/KjellKod/g3log
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * ============================================================================*/
#include <cstdio>
#include <string>

namespace solid {

// PUBLIC API:
/** Install signal handler that catches FATAL C-runtime or OS signals
    See the wikipedia site for details http://en.wikipedia.org/wiki/SIGFPE
    See the this site for example usage: http://www.tutorialspoint.com/cplusplus/cpp_signal_handling
    SIGABRT  ABORT (ANSI), abnormal termination
    SIGFPE   Floating point exception (ANSI)
    SIGILL   ILlegal instruction (ANSI)
    SIGSEGV  Segmentation violation i.e. illegal memory reference
    SIGTERM  TERMINATION (ANSI)  */
void install_crash_handler();

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
typedef unsigned long SignalType;
/// SIGFPE, SIGILL, and SIGSEGV handling must be installed per thread
/// on Windows. This is automatically done if you do at least one LOG(...) call
/// you can also use this function call, per thread so make sure these three
/// fatal signals are covered in your thread (even if you don't do a LOG(...) call
void install_signal_handler_for_thread();
#else
typedef int SignalType;
/// Probably only needed for unit testing. Resets the signal handling back to default
/// which might be needed in case it was previously overridden
/// The default signals are: SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGTERM
void restore_signal_handler_to_default();

std::string signal_to_string(int signal_number);

// restore to whatever signal handler was used before signal handler installation
void restore_signal_handler(int signal_number);

/// Overrides the existing signal handling for custom signals
/// For example: usage of zcmq relies on its own signal handler for SIGTERM
///     so users of g3log with zcmq should then use the @ref overrideSetupSignals
///     , likely with the original set of signals but with SIGTERM removed
///
/// call example:
///  g3::overrideSetupSignals({ {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},{SIGILL, "SIGILL"},
//                          {SIGSEGV, "SIGSEGV"},});
//void override_setup_signals(const std::map<int, std::string> overrideSignals);
#endif

namespace internal {
/** return whether or any fatal handling is still ongoing
       *  this is used by g3log::fatalCallToLogger
       *  only in the case of Windows exceptions (not fatal signals)
       *  are we interested in changing this from false to true to
       *  help any other exceptions handler work with 'EXCEPTION_CONTINUE_SEARCH'*/
bool should_block_for_fatal_handling();

/** \return signal_name Ref: signum.hpp and \ref installSignalHandler
      *  or for Windows exception name */
std::string exit_reason_name(const char* _text, SignalType signal_number);

/** return calling thread's stackdump*/
std::string stackdump(const char* dump = nullptr);

/** Re-"throw" a fatal signal, previously caught. This will exit the application
       * This is an internal only function. Do not use it elsewhere. It is triggered
       * from g3log, g3LogWorker after flushing messages to file */
//void exit_with_default_signal_handler(const LEVELS& level, g3::SignalType signal_number);
} //namespace internal
} //namespace solid
