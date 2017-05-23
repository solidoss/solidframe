// solid/system/debug.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"

#ifdef SOLID_HAS_STATISTICS

#define SOLID_COLLECT(om, ...) \
    om(__VA_ARGS__)

#else
#define SOLID_COLLECT(...)
#endif

#include "solid/system/common.hpp"
#include "solid/system/pimpl.hpp"
#include <ostream>
#include <string>

/*#ifdef SOLID_ON_WINDOWS

#ifdef DO_EXPORT_DLL
#define EXPORT_DLL __declspec(dllexport)
#else
#define EXPORT_DLL __declspec(dllimport)
#endif

#endif

#ifndef EXPORT_DLL
#define EXPORT_DLL
#endif
*/
namespace solid {

struct /*EXPORT_DLL*/ Debug {
    static const unsigned any;
    static const unsigned system;
    static const unsigned specific;
    static const unsigned proto_txt;
    static const unsigned proto_bin;
    static const unsigned ser_bin;
    static const unsigned utility;
    static const unsigned frame;
    static const unsigned mpipc;
    static const unsigned tcp;
    static const unsigned udp;
    static const unsigned file;
    static const unsigned log;
    static const unsigned aio;
    static Debug&         the();
    enum Level {
        Info      = 1,
        Error     = 2,
        Warn      = 4,
        Report    = 8,
        Verbose   = 16,
        Trace     = 32,
        AllLevels = 1 + 2 + 4 + 8 + 16 + 32
    };
    static void once_cbk();
    ~Debug();

    void initStdErr(
        bool         _buffered = true,
        std::string* _output   = nullptr);

    void initFile(
        const char*  _fname,
        bool         _buffered   = true,
        ulong        _respincnt  = 2,
        ulong        _respinsize = 1024 * 1024 * 1024,
        std::string* _output     = nullptr);
    void initSocket(
        const char*  _addr,
        const char*  _port,
        bool         _buffered = true,
        std::string* _output   = nullptr);

    void levelMask(const char* _msk = nullptr);
    void moduleMask(const char* _msk = nullptr);

    void moduleNames(std::string& _ros);
    void setAllModuleBits();
    void resetAllModuleBits();
    void setModuleBit(unsigned _v);
    void resetModuleBit(unsigned _v);

    unsigned registerModule(const char* _name);

    std::ostream& print();
    std::ostream& print(
        const char  _t,
        const char* _file,
        const char* _fnc,
        int         _line);
    std::ostream& print(
        const char  _t,
        unsigned    _module,
        const char* _file,
        const char* _fnc,
        int         _line);
    std::ostream& printTraceIn(
        const char  _t,
        unsigned    _module,
        const char* _file,
        const char* _fnc,
        int         _line);
    std::ostream& printTraceOut(
        const char  _t,
        unsigned    _module,
        const char* _file,
        const char* _fnc,
        int         _line);
    void done();
    void doneTraceIn();
    void doneTraceOut();
    bool isSet(Level _lvl, unsigned _v) const;

private:
    static Debug& dbg_the();
    Debug();
    Debug(Debug const&);
    Debug& operator=(Debug const&);

    struct Data;
    PimplT<Data> impl;
};

struct DebugTraceTest {
    DebugTraceTest(
        int         _mod,
        const char* _file,
        const char* _fnc,
        int         _line)
        : v(1)
        , mod(_mod)
        , file(_file)
        , fnc(_fnc)
        , line(_line)
    {
    }
    ~DebugTraceTest();
    int         v;
    int         mod;
    const char* file;
    const char* fnc;
    int         line;
};

} //namespace solid

#ifndef CRT_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define CRT_FUNCTION_NAME __func__
#else
#define CRT_FUNCTION_NAME __FUNCTION__
#endif
#endif

#ifdef SOLID_HAS_DEBUG

#define idbg(x)                                                                                            \
    do {                                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Info, solid::Debug::any)) {                            \
            solid::Debug::the().print('I', solid::Debug::any, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                                    \
        }                                                                                                  \
    } while (false);

#define idbgx(a, x)                                                                        \
    do {                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Info, a)) {                            \
            solid::Debug::the().print('I', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                    \
        }                                                                                  \
    } while (false);

#define edbg(x)                                                                                            \
    do {                                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Error, solid::Debug::any)) {                           \
            solid::Debug::the().print('E', solid::Debug::any, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                                    \
        }                                                                                                  \
    } while (false);

#define edbgx(a, x)                                                                        \
    do {                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Error, a)) {                           \
            solid::Debug::the().print('E', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                    \
        }                                                                                  \
    } while (false);

#define wdbg(x)                                                                                            \
    do {                                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Warn, solid::Debug::any)) {                            \
            solid::Debug::the().print('W', solid::Debug::any, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                                    \
        }                                                                                                  \
    } while (false);

#define wdbgx(a, x)                                                                        \
    do {                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Warn, a)) {                            \
            solid::Debug::the().print('W', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                    \
        }                                                                                  \
    } while (false);

#define rdbg(x)                                                                                            \
    do {                                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Report, solid::Debug::any)) {                          \
            solid::Debug::the().print('R', solid::Debug::any, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                                    \
        }                                                                                                  \
    } while (false);

#define rdbgx(a, x)                                                                        \
    do {                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Report, a)) {                          \
            solid::Debug::the().print('R', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                    \
        }                                                                                  \
    } while (false);

#define vdbg(x)                                                                                            \
    do {                                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Verbose, solid::Debug::any)) {                         \
            solid::Debug::the().print('V', solid::Debug::any, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                                    \
        }                                                                                                  \
    } while (false);

#define vdbgx(a, x)                                                                        \
    do {                                                                                   \
        if (solid::Debug::the().isSet(solid::Debug::Verbose, a)) {                         \
            solid::Debug::the().print('V', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().done();                                                    \
        }                                                                                  \
    } while (false);

#ifdef SOLID_HAS_DEBUG_TRACE
#define tdbgi(a, x)                                                                           \
    solid::DebugTraceTest __dbgtrace__(a, __FILE__, CRT_FUNCTION_NAME, __LINE__);             \
    if (solid::Debug::the().isSet(solid::Debug::Trace, a)) {                                  \
        solid::Debug::the().printTraceIn('T', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
        solid::Debug::the().doneTraceIn();                                                    \
    } else {                                                                                  \
        __dbgtrace__.v = 0;                                                                   \
    }

#define tdbgo(a, x)                                                                                \
    do {                                                                                           \
        if (solid::Debug::the().isSet(solid::Debug::Trace, a)) {                                   \
            __dbgtrace__.v = 0;                                                                    \
            solid::Debug::the().printTraceOut('T', a, __FILE__, CRT_FUNCTION_NAME, __LINE__) << x; \
            solid::Debug::the().doneTraceOut();                                                    \
        }                                                                                          \
    } while (false);

#else
#define tdbgi(a, x)
#define tdbgo(a, x)
#endif

#define pdbg(f, ln, fnc, x)                                                 \
    do {                                                                    \
        solid::Debug::the().print('P', solid::Debug::any, f, fnc, ln) << x; \
        solid::Debug::the().done();                                         \
    } while (false);

#define writedbg(x, sz)
#define writedbgx(a, x, sz)

#define check_call(a, v, c)                                                        \
    do {                                                                           \
        if ((c) != v) {                                                            \
            edbgx(a, "Error while calling ##c: " << last_system_error().message()) \
                SOLID_ASSERT(false);                                               \
        }                                                                          \
    } while (false);

#else

#define idbg(x)
#define idbgx(a, x)
#define edbg(x)
#define edbgx(a, x)
#define wdbg(x)
#define wdbgx(a, x)
#define rdbg(x)
#define rdbgx(a, x)
#define vdbg(x)
#define vdbgx(a, x)
#define tdbgi(a, x)
#define tdbgo(a, x)
#define pdbg(f, ln, fnc, x)

#define writedbg(x, sz)
#define writedbgx(a, x, sz)

#define check_call(a, v, c) c

#endif
