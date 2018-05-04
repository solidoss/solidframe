// solid/system/src/log.cpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/log.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <regex>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace solid {

//-----------------------------------------------------------------------------
//  Engine
//-----------------------------------------------------------------------------
namespace {

class NullBuf : public std::streambuf {
public:
    virtual std::streamsize xsputn(const char* s, std::streamsize n)
    {
        return n;
    }
    virtual int overflow(int c)
    {
        return 1;
    }
};

class NullOStream : public std::ostream {
public:
    NullOStream()
        : std::ostream(&buf)
    {
    }

private:
    NullBuf buf;
};

class Engine {
    struct ModuleStub {
        LoggerBase*            plgr_;
        const LogCategoryBase* plc_;

        ModuleStub(
            LoggerBase*            _plg = nullptr,
            const LogCategoryBase* _plc = nullptr)
            : plgr_(_plg)
            , plc_(_plc)
        {
        }

        bool empty() const
        {
            return plgr_ == nullptr;
        }
        void clear()
        {
            plgr_ = nullptr;
            plc_  = nullptr;
        }
    };

    using StringPairT       = std::pair<string, string>;
    using StringPairVectorT = std::vector<StringPairT>;
    using ModuleVectorT     = std::vector<ModuleStub>;

    mutex             mtx_;
    NullOStream       null_os_;
    ostream*          pos_;
    StringPairVectorT module_mask_vec_;
    ModuleVectorT     module_vec_;

public:
    static Engine& the()
    {
        static Engine e;
        return e;
    }

    Engine()
        : pos_(&null_os_)
    {
    }

    size_t registerLogger(LoggerBase& _rlg, const LogCategoryBase& _rlc);
    void   unregisterLogger(const size_t _idx);

    ostream& start(const size_t _idx);

    void done(const size_t _idx)
    {
        mtx_.unlock();
    }

    void configure(ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec);

    void reset()
    {
        lock_guard<mutex> lock(mtx_);
        pos_ = &null_os_;
    }

private:
    void doConfigureMasks(const std::vector<std::string>& _rmodule_mask_vec);
    void doConfigureModule(const size_t _idx);
};

size_t Engine::registerLogger(LoggerBase& _rlg, const LogCategoryBase& _rlc)
{
    lock_guard<mutex> lock(mtx_);
    module_vec_.emplace_back(&_rlg, &_rlc);
    return module_vec_.size() - 1;
}

void Engine::unregisterLogger(const size_t _idx)
{
    lock_guard<mutex> lock(mtx_);
    module_vec_[_idx].clear();
}

ostream& Engine::start(const size_t _idx)
{
    //if (impl_->respinsz && impl_->respinsz <= impl_->sz) {
    //    impl_->doRespin();
    //}
    mtx_.lock();
    return *pos_;
}

void Engine::configure(ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec)
{
    lock_guard<mutex> lock(mtx_);
    doConfigureMasks(_rmodule_mask_vec);
    pos_ = &_ros;
}

void Engine::doConfigureMasks(const std::vector<std::string>& _rmodule_mask_vec)
{
    string mdl;
    string msk;
    module_mask_vec_.clear();
    for (const auto& msk_str : _rmodule_mask_vec) {
        size_t off = msk_str.rfind(':');
        if (off != std::string::npos) {
            mdl = msk_str.substr(0, off);
            msk = msk_str.substr(off + 1);
        } else {
            mdl.clear();
            msk = msk_str;
        }
        module_mask_vec_.emplace_back(mdl, msk);
    }

    for (size_t i = 0; i < module_vec_.size(); ++i) {
        if (!module_vec_[i].empty()) {
            doConfigureModule(i);
        }
    }
}

void Engine::doConfigureModule(const size_t _idx)
{
    LogAtomicFlagsBackT msk_or  = 0;
    LogAtomicFlagsBackT msk_and = 0;
    for (const auto& mmp : module_mask_vec_) {
        if (mmp.first.empty()) {
            //always match
        } else {
            regex rgx(mmp.first);
            if (!regex_match(module_vec_[_idx].plgr_->name(), rgx)) {
                continue;
            }
        }
        LogAtomicFlagsBackT tmp_or_msk  = 0;
        LogAtomicFlagsBackT tmp_and_msk = 0;
        module_vec_[_idx].plc_->parse(tmp_or_msk, tmp_and_msk, mmp.second);
        msk_or |= tmp_or_msk;
        msk_and |= tmp_and_msk;
    }
    module_vec_[_idx].plgr_->remask(msk_or & (~msk_and));
}

const char* src_file_name(char const* _fname)
{
#ifdef SOLID_ON_WINDOWS
    static const bool    solid_found = strstr(__FILE__, "solid\\system\\src") != nullptr;
    static const size_t  fileoff     = (strlen(__FILE__) - strlen(strstr(__FILE__, "solid\\system\\src")));
    constexpr const char sep         = '\\';
#else
    static const bool    solid_found = strstr(__FILE__, "solid/system/src") != nullptr;
    static const size_t  fileoff     = (strlen(__FILE__) - strlen(strstr(__FILE__, "solid/system/src")));
    constexpr const char sep         = '/';
#endif

    if (solid_found) {
        if (
            _fname[fileoff + 0] == 's' && _fname[fileoff + 1] == 'o' && _fname[fileoff + 2] == 'l' && _fname[fileoff + 3] == 'i' && _fname[fileoff + 4] == 'd' && _fname[fileoff + 5] == sep) {
            return _fname + fileoff;
        }
    }
    const char* file_name = strrchr(_fname, sep);
    if (file_name != nullptr) {
        return file_name + 1;
    }
    return _fname;
}

} //namespace

//-----------------------------------------------------------------------------
//  LogCategory
//-----------------------------------------------------------------------------

/*virtual*/ LogCategoryBase::~LogCategoryBase() {}

/*static*/ const LogCategory& LogCategory::the()
{
    static const LogCategory lc;
    return lc;
}

void LogCategory::parse(LogAtomicFlagsBackT& _ror_flags, LogAtomicFlagsBackT& _rand_flags, const std::string& _txt) const
{

    for (const auto& c : _txt) {
        switch (c) {
        case 'v':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Verbose));
            break;
        case 'V':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Verbose));
            break;
        case 'i':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Info));
            break;
        case 'I':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Info));
            break;
        case 'e':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Error));
            break;
        case 'E':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Error));
            break;
        case 'w':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Warning));
            break;
        case 'W':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Warning));
            break;
        default:
            break;
        }
    }
}

LoggerBase::LoggerBase(const std::string& _name, const LogCategoryBase& _rlc)
    : name_(_name)
    , flags_(0)
    , idx_(Engine::the().registerLogger(*this, _rlc))
{
}

LoggerBase::~LoggerBase()
{
    flags_ = 0;
    Engine::the().unregisterLogger(idx_);
}

std::ostream& LoggerBase::doLog(const char* _flag_name, const char* _file, const char* _fnc, int _line) const
{
    constexpr size_t bufsz = 4 * 1024;
    char             buf[bufsz];
    const auto       now   = system_clock::now();
    time_t           t_now = system_clock::to_time_t(now);
    tm*              ploctm;
#ifdef SOLID_ON_WINDOWS
    struct tm loctm;
    localtime_s(&loctm, &t_now);
    ploctm = &loctm;
#else
    tm                   loctm;
    ploctm = localtime_r(&t_now, &loctm);
#endif
    int sz = snprintf(
        buf, bufsz,
        "%s[%04u-%02u-%02u %02u:%02u:%02u.%03u][%s][%s:%d %s][",
        _flag_name,
        ploctm->tm_year + 1900,
        ploctm->tm_mon + 1,
        ploctm->tm_mday,
        ploctm->tm_hour,
        ploctm->tm_min,
        ploctm->tm_sec,
        static_cast<unsigned int>(time_point_cast<milliseconds>(now).time_since_epoch().count() % 1000),
        name_.c_str(),
        src_file_name(_file),
        _line, _fnc);

    ostream& ros = Engine::the().start(idx_);

    ros.write(buf, sz);

#ifdef SOLID_ON_WINDOWS
    return ros << '[' << std::this_thread::get_id() << ']' << ' ';
#else
    return ros << "[0x" << std::hex << std::this_thread::get_id() << std::dec << ']' << ' ';
#endif
}

void LoggerBase::doDone() const
{
    Engine::the().done(idx_);
}

//-----------------------------------------------------------------------------
//  log_start
//-----------------------------------------------------------------------------
const LoggerT generic_logger{"*"};

void log_reset()
{
    Engine::the().reset();
}

ErrorConditionT log_start(std::ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec)
{
    Engine::the().configure(_ros, _rmodule_mask_vec);
    return ErrorConditionT();
}

ErrorConditionT log_start(
    const char*                     _fname,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered,
    uint32_t                        _respincnt,
    uint64_t                        _respinsize)
{
    return ErrorConditionT();
}

ErrorConditionT log_start(
    const char*                     _addr,
    const char*                     _port,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered)
{
    return ErrorConditionT();
}

} //namespace solid
