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
#include "solid/system/cassert.hpp"
#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/filedevice.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace solid {

std::ostream& operator<<(std::ostream& _ros, const LogLineBase& _line)
{
    return _line.writeTo(_ros);
}

LogLineBase::~LogLineBase() {}

LogRecorder::~LogRecorder() {}

void LogRecorder::recordLine(const solid::LogLineBase& /*_rlog_line*/) {}

void LogStreamRecorder::recordLine(const solid::LogLineBase& _rlog_line)
{
    _rlog_line.writeTo(ros_);
}

namespace {

enum {
    ErrorResolveE = 1,
    ErrorNoServerE,
    ErrorFileOpenE,
    ErrorPathE
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "solid::log";
    }
    std::string message(int _ev) const override;
};

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case ErrorResolveE:
        oss << "Resolve";
        break;
    case ErrorNoServerE:
        oss << "No Server";
        break;
    case ErrorFileOpenE:
        oss << "File Open";
        break;
    case ErrorPathE:
        oss << "Invalid Path";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

const ErrorCategory category;

const ErrorConditionT error_resolve(ErrorResolveE, category);
const ErrorConditionT error_no_server(ErrorNoServerE, category);
const ErrorConditionT error_file_open(ErrorFileOpenE, category);
const ErrorConditionT error_path(ErrorPathE, category);

//-----------------------------------------------------------------------------
//  DeviceBasicStream
//-----------------------------------------------------------------------------

class DeviceBasicBuffer : public std::streambuf {
public:
    // constructor
    DeviceBasicBuffer(uint64_t& _rsz)
        : rsz_(_rsz)
    {
    }

    void device(Device&& _udev)
    {
        dev_ = std::move(_udev);
    }

    void swapDevice(Device& _rdev)
    {
        std::swap(_rdev, dev_);
    }

protected:
    bool writeAll(const char* _s, size_t _n)
    {
        do {
            ssize_t written = dev_.write(_s, _n);
            if (written >= 0) {
                _n -= written;
                _s += written;
                rsz_ += written;
            } else {
                return false;
            }
        } while (_n != 0);
        return true;
    }

    // write one character
    int_type overflow(int_type c) override
    {
        if (c != EOF) {
            char z = c;
            if (!writeAll(&z, 1)) {
                return EOF;
            }
            ++rsz_;
        }
        return c;
    }

    // write multiple characters
    std::streamsize xsputn(const char* s, std::streamsize num) override
    {
        if (writeAll(s, num)) {
            return num;
        }
        return -1;
    }

private:
    Device    dev_;
    uint64_t& rsz_;
};

//-----------------------------------------------------------------------------

class DeviceBasicStream : public std::ostream {
protected:
    DeviceBasicBuffer buf;

public:
    DeviceBasicStream(uint64_t& _rsz)
        : std::ostream(nullptr)
        , buf(_rsz)
    {
        rdbuf(&buf);
    }

    void device(Device&& _udev)
    {
        buf.device(std::move(_udev));
    }

    void swapDevice(Device& _rdev)
    {
        buf.swapDevice(_rdev);
    }
};

//-----------------------------------------------------------------------------
//  DeviceStream
//-----------------------------------------------------------------------------

class DeviceBuffer : public std::streambuf {
public:
    constexpr static const size_t buffer_capacity = 2048;
    constexpr static const size_t buffer_flush    = 1024;

    // constructor
    DeviceBuffer(uint64_t& _rsz)
        : rsz_(_rsz)
        , bpos(bbeg)
    {
    }

    void device(Device&& _udev)
    {
        dev_ = std::move(_udev);
    }

    void swapDevice(Device& _rdev)
    {
        flush();
        std::swap(_rdev, dev_);
    }

protected:
    // write one character
    int_type overflow(int_type c) override;
    // write multiple characters
    std::streamsize xsputn(const char* s, std::streamsize num) override;

private:
    bool writeAll(const char* _s, size_t _n)
    {
        do {
            ssize_t written = dev_.write(_s, _n);
            if (written >= 0) {
                _n -= written;
                _s += written;
            } else {
                return false;
            }
        } while (_n != 0);
        return true;
    }

    bool flush()
    {
        ptrdiff_t towrite = bpos - bbeg;
        bpos              = bbeg;
        if (!writeAll(bbeg, towrite)) {
            return false;
        }
        rsz_ += towrite;
        return true;
    }

private:
    Device    dev_;
    uint64_t& rsz_;
    char      bbeg[buffer_capacity];
    char*     bpos;
};
//-----------------------------------------------------------------------------
std::streambuf::int_type DeviceBuffer::overflow(int_type c)
{
    if (c != EOF) {
        *bpos = c;
        ++bpos;
        if (static_cast<size_t>(bpos - bbeg) > buffer_flush && !flush()) {
            return EOF;
        }
    }
    return c;
}
//-----------------------------------------------------------------------------
// write multiple characters
std::streamsize DeviceBuffer::xsputn(const char* s, std::streamsize num)
{
    // we can safely put BUFF_FLUSH into the buffer
    long towrite = buffer_capacity - buffer_flush;
    if (towrite > static_cast<int>(num)) {
        towrite = static_cast<int>(num);
    }
    memcpy(bpos, s, towrite);
    bpos += towrite;
    if (static_cast<size_t>(bpos - bbeg) > buffer_flush && !flush()) {
        return -1;
    }
    if (num == towrite) {
        return num;
    }
    num -= towrite;
    s += towrite;
    if (num >= static_cast<std::streamsize>(buffer_flush)) {
        std::streamsize retv = dev_.write(s, static_cast<uint32_t>(num));
        solid_assert(retv != num);
        return retv;
    }
    memcpy(bpos, s, static_cast<size_t>(num));
    bpos += num;
    return num;
}

class DeviceStream : public std::ostream {
protected:
    DeviceBuffer buf;

public:
    DeviceStream(uint64_t& _rsz)
        : std::ostream(nullptr)
        , buf(_rsz)
    {
        rdbuf(&buf);
    }

    void device(Device&& _udev)
    {
        buf.device(std::move(_udev));
    }

    void swapDevice(Device& _rdev)
    {
        buf.swapDevice(_rdev);
    }
};

struct SocketRecorder : LogRecorder {
    uint64_t          current_size_;
    DeviceBasicStream unbuffered_stream_;
    DeviceStream      buffered_stream_;
    std::ostream&     ros_;

    std::ostream& stream(const bool _buffered, SocketDevice&& _rsd)
    {
        if (_buffered) {
            buffered_stream_.device(std::move(_rsd));
            return buffered_stream_;
        }
        unbuffered_stream_.device(std::move(_rsd));
        return unbuffered_stream_;
    }

    SocketRecorder(
        SocketDevice&& _rsd, const bool _buffered)
        : current_size_(0)
        , unbuffered_stream_(current_size_)
        , buffered_stream_(current_size_)
        , ros_(stream(_buffered, std::move(_rsd)))
    {
    }

    ~SocketRecorder() override
    {
        SocketDevice sd;
        unbuffered_stream_.swapDevice(sd);

        if (!sd) {
            buffered_stream_.swapDevice(sd);
        }
        sd.shutdownReadWrite();
    }

    void recordLine(const solid::LogLineBase& _rlog_line) override
    {
        _rlog_line.writeTo(ros_);
    }
};

#ifdef SOLID_ON_WINDOWS
constexpr const char path_separator = '\\';
#else
constexpr const char path_separator = '/';
#endif

void filePath(string& _out, uint32_t _pos, const string& _path, const string& _name)
{
    constexpr size_t bufcp = 4096;
    char             buf[bufcp];

    _out = _path;
    _out += _name;

    if (_pos != 0u) {
        snprintf(buf, bufcp, "_%04lu.log", static_cast<unsigned long>(_pos));
    } else {
        snprintf(buf, bufcp, ".log");
    }
    _out += buf;
}

void splitPrefix(string& _path, string& _name, const char* _prefix)
{
    const char* p = strrchr(_prefix, path_separator);
    if (p == nullptr) {
        _name = _prefix;
    } else {
        _path.assign(_prefix, (p - _prefix) + 1);
        _name = (p + 1);
    }
}

struct FileRecorder : LogRecorder {
    uint64_t          current_size_;
    DeviceBasicStream unbuffered_stream_;
    DeviceStream      buffered_stream_;
    std::ostream&     ros_;
    const std::string path_;
    const std::string name_;
    const uint64_t    respin_size_;
    const uint32_t    respin_count_;

    std::ostream& stream(const bool _buffered, FileDevice&& _rsd)
    {
        if (_buffered) {
            buffered_stream_.device(std::move(_rsd));
            return buffered_stream_;
        }
        unbuffered_stream_.device(std::move(_rsd));
        return unbuffered_stream_;
    }

    FileRecorder(
        FileDevice&&   _rfd,
        std::string&&  _path,
        std::string&&  _name,
        const uint64_t _respinsize,
        const uint32_t _respincnt,
        const bool     _buffered)
        : current_size_(_rfd.size()) // inherit the current size of the file
        , unbuffered_stream_(current_size_)
        , buffered_stream_(current_size_)
        , ros_(stream(_buffered, std::move(_rfd)))
        , path_(std::move(_path))
        , name_(std::move(_name))
        , respin_size_(_respinsize)
        , respin_count_(_respincnt)
    {
    }

    ~FileRecorder() override
    {
        FileDevice fd;
        unbuffered_stream_.swapDevice(fd);

        if (!fd) {
            buffered_stream_.swapDevice(fd);
        }
        fd.flush();
    }

    bool shouldRespin(const size_t _sz) const
    {
        return (respin_size_ != 0u) && respin_size_ < (current_size_ + _sz);
    }

    void doRespin()
    {
        current_size_ = 0;
        string fname;

        FileDevice fd;
        bool       buffered = false;
        unbuffered_stream_.swapDevice(fd);

        if (!fd) {
            buffered_stream_.swapDevice(fd);
            buffered = true;
        }
        fd.flush();
        fd.close();

        // find the last file
        if (respin_count_ == 0) {
            filePath(fname, 0, path_, name_);
            Directory::eraseFile(fname.c_str());
        } else {
            uint32_t lastpos = respin_count_;
            bool     do_move = false;

            while (lastpos > 1) {
                filePath(fname, lastpos, path_, name_);
                if (!do_move) {
                    if (FileDevice::size(fname.c_str()) == 0) {
                        --lastpos;
                        continue;
                    } else {
                        if (lastpos == respin_count_) {
                            do_move = true;
                            continue;
                        } else {
                            ++lastpos; // use the previous position to move the current log file to
                            break;
                        }
                    }
                } else {
                    auto&  from_path = fname;
                    string to_path;
                    filePath(to_path, lastpos - 1, path_, name_);
                    Directory::renameFile(from_path.c_str(), to_path.c_str());
                    --lastpos;
                }
            }
            if (do_move) {
                lastpos = respin_count_;
            }
            string from_path;
            string to_path;
            filePath(from_path, 0, path_, name_);
            filePath(to_path, lastpos, path_, name_);
            Directory::renameFile(from_path.c_str(), to_path.c_str());
            fname = from_path;
        }

        if (!fd.create(fname.c_str(), FileDevice::WriteOnlyE)) {
            cerr << "Cannot create log file: " << fname << ": " << last_system_error().message() << endl;
            solid_throw("Cannot create log file: " << fname << ": " << last_system_error().message());
        }
        stream(buffered, std::move(fd));
    }

    void recordLine(const solid::LogLineBase& _rlog_line) override
    {
        if (shouldRespin(_rlog_line.size())) {
            doRespin();
        }
        _rlog_line.writeTo(ros_);
    }
};

//-----------------------------------------------------------------------------
//  Engine
//-----------------------------------------------------------------------------

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
    StringPairVectorT module_mask_vec_;
    ModuleVectorT     module_vec_;
    LogRecorderPtrT   recorder_ptr_;

public:
    static Engine& the()
    {
        static Engine e;
        return e;
    }

    Engine()
        : recorder_ptr_(std::make_shared<LogRecorder>())

    {
    }

    ~Engine()
    {
        close();
    }

    size_t registerLogger(LoggerBase& _rlg, const LogCategoryBase& _rlc);
    void   unregisterLogger(size_t _idx);

    void log(size_t _idx, const LogLineBase& _log_ros);

    ErrorConditionT configure(LogRecorderPtrT&& _recorder_ptr, const std::vector<std::string>& _rmodule_mask_vec);

    void close()
    {
        lock_guard<mutex> lock(mtx_);
        recorder_ptr_ = std::make_shared<LogRecorder>();
    }

private:
    void doConfigureMasks(const std::vector<std::string>& _rmodule_mask_vec);
    void doConfigureModule(size_t _idx);
};

size_t Engine::registerLogger(LoggerBase& _rlg, const LogCategoryBase& _rlc)
{
    lock_guard<mutex> lock(mtx_);
    module_vec_.emplace_back(&_rlg, &_rlc);
    size_t idx = module_vec_.size() - 1;

    doConfigureModule(idx);
    return idx;
}

void Engine::unregisterLogger(const size_t _idx)
{
    lock_guard<mutex> lock(mtx_);
    module_vec_[_idx].clear();
}

void Engine::log(const size_t /*_idx*/, const LogLineBase& _log_ros)
{
    std::lock_guard<std::mutex> lock(mtx_);
    recorder_ptr_->recordLine(_log_ros);
}

ErrorConditionT Engine::configure(LogRecorderPtrT&& _recorder_ptr, const std::vector<std::string>& _rmodule_mask_vec)
{
    lock_guard<mutex> lock(mtx_);
    doConfigureMasks(_rmodule_mask_vec);
    recorder_ptr_ = std::move(_recorder_ptr);
    return ErrorConditionT();
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
            // always match
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
    static const bool   solid_found = strstr(__FILE__, "solid\\system\\src") != nullptr;
    static const size_t fileoff     = (strlen(__FILE__) - strlen(strstr(__FILE__, "solid\\system\\src")));
#else
    static const bool   solid_found = strstr(__FILE__, "solid/system/src") != nullptr;
    static const size_t fileoff     = (strlen(__FILE__) - strlen(strstr(__FILE__, "solid/system/src")));
#endif

    if (solid_found) {
        if (
            _fname[fileoff + 0] == 's' && _fname[fileoff + 1] == 'o' && _fname[fileoff + 2] == 'l' && _fname[fileoff + 3] == 'i' && _fname[fileoff + 4] == 'd' && _fname[fileoff + 5] == path_separator) {
            return _fname + fileoff;
        }
    }
    const char* file_name = strrchr(_fname, path_separator);
    if (file_name != nullptr) {
        return file_name + 1;
    }
    return _fname;
}

} // namespace

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
        case 's':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Statistic));
            break;
        case 'S':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Statistic));
            break;
        case 'r':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Raw));
            break;
        case 'R':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Raw));
            break;
        case 'x':
            _rand_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Exception));
            break;
        case 'X':
            _ror_flags |= (1UL << static_cast<LogAtomicFlagsBackT>(LogFlags::Exception));
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

std::ostream& LoggerBase::doLog(std::ostream& _ros, const char* _flag_name, const char* _file, const char* _fnc, int _line) const
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
    tm loctm;
    ploctm = localtime_r(&t_now, &loctm);
#endif
    int sz = snprintf(
        buf, bufsz,
        "%s[%04u-%02u-%02u %02u:%02u:%02u.%03u][%s][%s:%d %s]",
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

    _ros.write(buf, sz);

#ifdef SOLID_ON_WINDOWS
    return _ros << '[' << std::this_thread::get_id() << ']' << ' ';
#else
    return _ros << "[0x" << std::hex << std::this_thread::get_id() << std::dec << ']' << ' ';
#endif
}

void LoggerBase::doDone(const LogLineBase& _log_ros) const
{
    Engine::the().log(idx_, _log_ros);
}

//-----------------------------------------------------------------------------
//  log_start
//-----------------------------------------------------------------------------
const LoggerT generic_logger{"*"};

void log_stop()
{
    Engine::the().close();
}

ErrorConditionT log_start(LogRecorderPtrT&& _rec_ptr, const std::vector<std::string>& _rmodule_mask_vec)
{
    return Engine::the().configure(std::move(_rec_ptr), _rmodule_mask_vec);
}

ErrorConditionT log_start(std::ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec)
{
    return Engine::the().configure(std::make_shared<LogStreamRecorder>(_ros), _rmodule_mask_vec);
}

ErrorConditionT log_start(
    const char*                     _prefix,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered,
    uint32_t                        _respincnt,
    uint64_t                        _respinsize)
{
    FileDevice  fd;
    std::string path;
    std::string name;

    if (_prefix != nullptr && (*_prefix != 0)) {
        splitPrefix(path, name, _prefix);
        if (path.empty()) {
            path = "log";
            path += path_separator;
        }
        Directory::create_all(path.c_str());
        string fpath;

        filePath(fpath, 0, path, name);

        if (!fd.open(fpath.c_str(), FileDevice::WriteOnlyE | FileDevice::CreateE | FileDevice::AppendE)) {
            return error_file_open;
        }
    } else {
        return error_path;
    }

    return Engine::the().configure(std::make_shared<FileRecorder>(std::move(fd), std::move(path), std::move(name), _respinsize, _respincnt, _buffered), _rmodule_mask_vec);
}

ErrorConditionT log_start(
    const char*                     _addr,
    const char*                     _port,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered)
{

    ResolveData  rd = synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
    SocketDevice sd;

    if (!rd.empty()) {
        auto it = rd.begin();
        while (it != rd.end()) {
            if (!sd.create(it) && !sd.connect(it)) {
                break;
            }
            sd.close();
            ++it;
        }
        if (!sd) {
            return error_no_server;
        }
    } else {
        return error_resolve;
    }

    return Engine::the().configure(std::make_shared<SocketRecorder>(std::move(sd), _buffered), _rmodule_mask_vec);
}

} // namespace solid
