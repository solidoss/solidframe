// solid/system/log.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/chunkedstream.hpp"
#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include <atomic>
#include <cstring>
#include <memory>
#include <ostream>
#include <string>

namespace solid {

using LogAtomicFlagsBackT = unsigned long;
using LogAtomicFlagsT     = std::atomic<LogAtomicFlagsBackT>;

enum struct LogFlags : LogAtomicFlagsBackT {
    Verbose,
    Info,
    Warning,
    Error,
    Statistic,
    Raw,
    LastFlag
};

struct LogCategoryBase {
    virtual ~LogCategoryBase();

    virtual void parse(LogAtomicFlagsBackT& _ror_flags, LogAtomicFlagsBackT& _rand_flags, const std::string& _txt) const = 0;
};

struct LogCategory : public LogCategoryBase {
    static const LogCategory& the();

    const char* flagName(const LogFlags _flag) const
    {
        switch (_flag) {
        case LogFlags::Verbose:
            return "V";
        case LogFlags::Info:
            return "I";
        case LogFlags::Warning:
            return "W";
        case LogFlags::Error:
            return "E";
        case LogFlags::Statistic:
            return "S";
        case LogFlags::Raw:
            return "R";
        case LogFlags::LastFlag:
            break;
        }
        return "?";
    }

private:
    void parse(LogAtomicFlagsBackT& _ror_flags, LogAtomicFlagsBackT& _rand_flags, const std::string& _txt) const override;
};

struct LogLineBase {
    virtual ~LogLineBase();
    virtual std::ostream& writeTo(std::ostream&) const = 0;
    virtual size_t        size() const                 = 0;
};

namespace impl {

template <size_t Size>
class LogLineStream : public OChunkedStream<Size, Size>, public LogLineBase {
    using BaseStream = OChunkedStream<Size, Size>;

public:
    std::ostream& writeTo(std::ostream& _ros) const override
    {
        return BaseStream::writeTo(_ros);
    }
    size_t size() const override
    {
        return BaseStream::size();
    }
};

} //namespace impl

std::ostream& operator<<(std::ostream& _ros, const LogLineBase& _line);

class LoggerBase : NonCopyable {
    //friend class Engine;

    const std::string name_;
    LogAtomicFlagsT   flags_;
    const size_t      idx_;

protected:
    LoggerBase(const std::string& _name, const LogCategoryBase& _rlc);
    ~LoggerBase();

    std::ostream& doLog(std::ostream& _ros, const char* _flag_name, const char* _file, const char* _fnc, int _line) const;
    void          doDone(const LogLineBase& _log_ros) const;

public:
    const std::string& name() const
    {
        return name_;
    }
    void remask(const LogAtomicFlagsBackT _msk)
    {
        flags_.store(_msk);
    }
    LogAtomicFlagsBackT flags() const
    {
        return flags_.load(std::memory_order_relaxed);
    }
};

template <class Flgs = LogFlags, class LogCat = LogCategory>
class Logger : protected LoggerBase {
    const LogCat& rcat_;

public:
    using ThisT = Logger<Flgs, LogCat>;
    using FlagT = Flgs;

    Logger(const std::string& _name)
        : LoggerBase(_name, LogCat::the())
        , rcat_(LogCat::the())
    {
    }

    bool shouldLog(const FlagT _flag) const
    {
        return (flags() & (1UL << static_cast<size_t>(_flag))) != 0;
    }

    std::ostream& log(std::ostream& _ros, const FlagT _flag, const char* _file, const char* _fnc, int _line) const
    {
        return this->doLog(_ros, rcat_.flagName(_flag), _file, _fnc, _line);
    }
    void done(const LogLineBase& _log_ros) const
    {
        doDone(_log_ros);
    }
};

using LoggerT = Logger<>;

extern const LoggerT generic_logger;

void log_stop();

struct LogRecorder : NonCopyable {
    virtual ~LogRecorder();

    virtual void recordLine(const solid::LogLineBase& /*_rlog_line*/);
};

struct LogStreamRecorder : LogRecorder {
    std::ostream& ros_;
    LogStreamRecorder(std::ostream& _ros)
        : ros_(_ros)
    {
    }

    void recordLine(const solid::LogLineBase& _rlog_line) override;
};

using LogRecorderPtrT = std::shared_ptr<LogRecorder>;

ErrorConditionT log_start(
    LogRecorderPtrT&&               _rec_ptr,
    const std::vector<std::string>& _rmodule_mask_vec);

ErrorConditionT log_start(
    std::ostream&                   _ros,
    const std::vector<std::string>& _rmodule_mask_vec);

ErrorConditionT log_start(
    const char*                     _fname,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered   = true,
    uint32_t                        _respincnt  = 2,
    uint64_t                        _respinsize = 1024 * 1024 * 1024);

ErrorConditionT log_start(
    const char*                     _addr,
    const char*                     _port,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered = true);

} //namespace solid

#ifndef SOLID_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define SOLID_FUNCTION_NAME __func__
#else
#define SOLID_FUNCTION_NAME __FUNCTION__
#endif
#endif

#ifndef SOLID_LOG_BUFFER_SIZE
#define SOLID_LOG_BUFFER_SIZE 2 * 1024
#endif

#ifdef SOLID_HAS_DEBUG

#define solid_dbg(Lgr, Flg, Txt)                                                                                                         \
    if (Lgr.shouldLog(decltype(Lgr)::FlagT::Flg)) {                                                                                      \
        solid::impl::LogLineStream<SOLID_LOG_BUFFER_SIZE> os;                                                                            \
        Lgr.log(os, decltype(Lgr)::FlagT::Flg, __FILE__, static_cast<const char*>((SOLID_FUNCTION_NAME)), __LINE__) << Txt << std::endl; \
        Lgr.done(os);                                                                                                                    \
    }

#else

#define solid_dbg(...)

#endif

#define solid_log(Lgr, Flg, Txt)                                                                                                         \
    if (Lgr.shouldLog(decltype(Lgr)::FlagT::Flg)) {                                                                                      \
        solid::impl::LogLineStream<SOLID_LOG_BUFFER_SIZE> os;                                                                            \
        Lgr.log(os, decltype(Lgr)::FlagT::Flg, __FILE__, static_cast<const char*>((SOLID_FUNCTION_NAME)), __LINE__) << Txt << std::endl; \
        Lgr.done(os);                                                                                                                    \
    }

#define solid_log_raw(Txt)                                       \
    if (solid::generic_logger.shouldLog(solid::LogFlags::Raw)) { \
        solid::impl::LogLineStream<SOLID_LOG_BUFFER_SIZE> os;    \
        os << Txt;                                               \
    Lgr.done(os)

#ifdef SOLID_HAS_STATISTICS

#define solid_collect(om, ...) \
    om(__VA_ARGS__)

#else
#define solid_collect(...)
#endif
