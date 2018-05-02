// solid/system/log.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include <ostream>
#include <string>
#include <atomic>

namespace solid {

enum struct LogFlags : size_t {
    Verbose,
    Info,
    Warning,
    Error,
    LastFlag
};

using LogAtomicFlagsT = std::atomic<unsigned long>;

struct LogCategory {
    static const LogCategory& the();
    
    void parse(LogAtomicFlagsT &_rflags, const std::string &_txt)const;
    const char *flagName(const LogFlags _flag)const{
        return "test";
    }
};

class LoggerBase {
    const std::string  name_;
    const size_t       idx_;
protected:
    LoggerBase(const std::string& _name):name_(_name), idx_(0){}
    
    std::ostream& doLog(const char *_flag_name, const char* _file, const char* _fnc, int _line)const{}
    void doDone()const{}
};

template <class Flgs = LogFlags, class LogCat = LogCategory>
class Logger : protected LoggerBase {
    LogAtomicFlagsT     flags_;
    const LogCat&       rcat_;
public:
    using ThisT = Logger<Flgs, LogCat>;
    using FlagT = Flgs;

    Logger(const std::string& _name)
        : LoggerBase(_name), flags_(0), rcat_(LogCat::the())
    {
    }

    bool shouldLog(const FlagT _flag) const
    {
        return (flags_.load(std::memory_order_relaxed) & (1UL << static_cast<size_t>(_flag))) != 0;
    }

    std::ostream& log(const FlagT _flag, const char* _file, const char* _fnc, int _line) const
    {
        return this->doLog(rcat_.flagName(_flag), _file, _fnc, _line);
    }
    void done() const {
        doDone();
    }
private:
    void setFlags(const std::string &_txt){
        rcat_.parse(flags_, _txt);
    }
};

using LoggerT = Logger<>;

extern const LoggerT basic_logger;

ErrorConditionT log_start(std::ostream& _ros, const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec);

ErrorConditionT log_start(
    const char*        _fname,
    const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec,
    bool     _buffered   = true,
    uint32_t _respincnt  = 2,
    uint64_t _respinsize = 1024 * 1024 * 1024);

ErrorConditionT log_start(
    const char*        _addr,
    const char*        _port,
    const std::string& _rmask, std::vector<std::string>&& _rmodule_mask_vec,
    bool _buffered = true);

} //namespace solid

#ifndef SOLID_FUNCTION_NAME
#ifdef SOLID_ON_WINDOWS
#define SOLID_FUNCTION_NAME __func__
#else
#define SOLID_FUNCTION_NAME __FUNCTION__
#endif
#endif

#ifdef SOLID_HAS_DEBUG

#define solid_dbg(Lgr, Flg, Txt)                                                            \
    if (Lgr.shouldLog(decltype(Lgr)::FlagT::Flg)) {                                         \
        Lgr.log(decltype(Lgr)::FlagT::Flg, __FILE__, SOLID_FUNCTION_NAME, __LINE__) << Txt; \
        Lgr.done();                                                                         \
    }

#else

#define solid_dbg(...)

#endif

#define solid_log(Lgr, Flg, Txt)                                                            \
    if (Lgr.shouldLog(decltype(Lgr)::FlagT::Flg)) {                                         \
        Lgr.log(decltype(Lgr)::FlagT::Flg, __FILE__, SOLID_FUNCTION_NAME, __LINE__) << Txt; \
        Lgr.done();                                                                         \
    }
