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
#include "solid/system/filedevice.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace solid {

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
    const char* name() const noexcept
    {
        return "solid::log";
    }
    std::string message(int _ev) const;
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
//  NullStream
//-----------------------------------------------------------------------------

class NullBuf : public std::streambuf {
public:
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        return n;
    }
    int overflow(int c) override
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
    // write one character
    int_type overflow(int_type c) override
    {
        if (c != EOF) {
            char z = c;
            if (dev_.write(&z, 1) != 1) {
                return EOF;
            }
            ++rsz_;
        }
        return c;
    }
    // write multiple characters
    std::streamsize xsputn(const char* s, std::streamsize num) override
    {
        std::streamsize sz = dev_.write(s, static_cast<uint32_t>(num));
        rsz_ += sz;
        return sz;
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
        : std::ostream(0)
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
    bool flush()
    {
        ptrdiff_t towrite = bpos - bbeg;
        bpos              = bbeg;
        if (dev_.write(bbeg, towrite) != towrite)
            return false;
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
    //we can safely put BUFF_FLUSH into the buffer
    long towrite = buffer_capacity - buffer_flush;
    if (towrite > static_cast<int>(num))
        towrite = static_cast<int>(num);
    memcpy(bpos, s, towrite);
    bpos += towrite;
    if (static_cast<size_t>(bpos - bbeg) > buffer_flush && !flush()) {
        SOLID_ASSERT(0);
        return -1;
    }
    if (num == towrite)
        return num;
    num -= towrite;
    s += towrite;
    if (num >= static_cast<std::streamsize>(buffer_flush)) {
        std::streamsize retv = dev_.write(s, static_cast<uint32_t>(num));
        SOLID_ASSERT(retv != num);
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
        : std::ostream(0)
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
    NullOStream       null_os_;
    ostream*          pos_;
    StringPairVectorT module_mask_vec_;
    ModuleVectorT     module_vec_;
    uint64_t          respin_size_;
    uint32_t          respin_count_;
    uint32_t          respin_pos_;
    uint64_t          current_size_;
    DeviceBasicStream basic_stream_;
    DeviceStream      stream_;
    bool              is_socket_;
    string            path_;
    string            name_;

public:
    static Engine& the()
    {
        static Engine e;
        return e;
    }

    Engine()
        : pos_(&null_os_)
        , respin_size_(0)
        , respin_count_(0)
        , current_size_(0)
        , basic_stream_(current_size_)
        , stream_(current_size_)
        , is_socket_(false)

    {
    }

    ~Engine()
    {
        close();
    }

    size_t registerLogger(LoggerBase& _rlg, const LogCategoryBase& _rlc);
    void   unregisterLogger(const size_t _idx);

    ostream& start(const size_t _idx);

    void done(const size_t _idx)
    {
        mtx_.unlock();
    }

    ErrorConditionT configure(ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec);
    ErrorConditionT configure(
        const char*                     _fname,
        const std::vector<std::string>& _rmodule_mask_vec,
        bool                            _buffered,
        uint32_t                        _respincnt,
        uint64_t                        _respinsize);
    ErrorConditionT configure(
        const char*                     _addr,
        const char*                     _port,
        const std::vector<std::string>& _rmodule_mask_vec,
        bool                            _buffered);
    void reset()
    {
        lock_guard<mutex> lock(mtx_);
        pos_ = &null_os_;
    }

    void close()
    {
        FileDevice   fd;
        SocketDevice sd;
        {
            lock_guard<mutex> lock(mtx_);
            if (pos_ == &basic_stream_) {
                if (is_socket_) {
                    basic_stream_.swapDevice(sd);
                } else {
                    basic_stream_.swapDevice(fd);
                }
            } else if (pos_ == &stream_) {
                if (is_socket_) {
                    stream_.swapDevice(sd);
                } else {
                    stream_.swapDevice(fd);
                }
            }
            pos_ = &null_os_;
        }
        if (fd) {
            fd.flush();
        }
        if (sd) {
            sd.shutdownReadWrite();
        }
    }

private:
    bool shouldRespin() const
    {
        return respin_size_ && respin_size_ < current_size_;
    }

    void doConfigureMasks(const std::vector<std::string>& _rmodule_mask_vec);
    void doConfigureModule(const size_t _idx);
    void doRespin();

    void doClose()
    {
        FileDevice   fd;
        SocketDevice sd;
        {
            if (pos_ == &basic_stream_) {
                if (is_socket_) {
                    basic_stream_.swapDevice(sd);
                } else {
                    basic_stream_.swapDevice(fd);
                }
            } else if (pos_ == &stream_) {
                if (is_socket_) {
                    stream_.swapDevice(sd);
                } else {
                    stream_.swapDevice(fd);
                }
            }
            pos_ = &null_os_;
        }
        if (fd) {
            fd.flush();
        }
        if (sd) {
            sd.shutdownReadWrite();
        }
        is_socket_ = false;
    }
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
    if (shouldRespin()) {
        doRespin();
    }
    mtx_.lock();
    return *pos_;
}

ErrorConditionT Engine::configure(ostream& _ros, const std::vector<std::string>& _rmodule_mask_vec)
{
    lock_guard<mutex> lock(mtx_);
    doConfigureMasks(_rmodule_mask_vec);
    pos_ = &_ros;
    return ErrorConditionT();
}

#ifdef SOLID_ON_WINDOWS
constexpr const char path_separator = '\\';
#else
constexpr const char path_separator = '/';
#endif

void filePath(string& _out, uint32_t _pos, const string& _path, const string& _name)
{
    constexpr size_t bufcp = 2048;
    char             buf[bufcp];

    _out = _path;
    _out += _name;

    if (_pos) {
        snprintf(buf, bufcp, "_%04lu.log", (unsigned long)_pos);
    } else {
        snprintf(buf, bufcp, ".log");
    }
    _out += buf;
}

void splitPrefix(string& _path, string& _name, const char* _prefix)
{
    const char* p = strrchr(_prefix, path_separator);
    if (!p) {
        _name = _prefix;
    } else {
        _path.assign(_prefix, (p - _prefix) + 1);
        _name = (p + 1);
    }
}

ErrorConditionT Engine::configure(
    const char*                     _prefix,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered,
    uint32_t                        _respincnt,
    uint64_t                        _respinsize)
{
    FileDevice fd;

    lock_guard<mutex> lock(mtx_);

    doClose();

    if (_prefix && *_prefix) {
        splitPrefix(path_, name_, _prefix);
        if (path_.empty()) {
            path_ = "log";
            path_ += path_separator;
        }
        Directory::create(path_.c_str());
        string fpath;

        filePath(fpath, 0, path_, name_);

        if (!fd.open(fpath.c_str(), FileDevice::WriteOnlyE | FileDevice::CreateE | FileDevice::AppendE)) {
            path_.clear();
            name_.clear();
            return error_file_open;
        }
    } else {
        path_.clear();
        name_.clear();
        return error_path;
    }

    doConfigureMasks(_rmodule_mask_vec);

    current_size_ = fd.size(); //set the current size to the size of the file

    respin_count_ = _respincnt;
    respin_size_  = _respinsize;

    if (_buffered) {
        stream_.device(std::move(fd));
        pos_ = &stream_;
    } else {
        basic_stream_.device(std::move(fd));
        pos_ = &basic_stream_;
    }

    return ErrorConditionT();
}

ErrorConditionT Engine::configure(
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
            } else {
                sd.close();
            }
            ++it;
        }
        if (!sd) {
            return error_no_server;
        }
    } else {
        return error_resolve;
    }

    lock_guard<mutex> lock(mtx_);

    doClose();

    doConfigureMasks(_rmodule_mask_vec);

    is_socket_ = true;

    if (_buffered) {
        stream_.device(std::move(sd));
        pos_ = &stream_;
    } else {
        basic_stream_.device(std::move(sd));
        pos_ = &basic_stream_;
    }
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

void Engine::doRespin()
{
    current_size_ = 0;
    string fname;

    FileDevice fd;
    if (pos_ == &basic_stream_) {
        basic_stream_.swapDevice(fd);
    } else if (pos_ == &stream_) {
        stream_.swapDevice(fd);
    } else {
        SOLID_ASSERT(false);
    }

    fd.flush();
    fd.close();

    //find the last file
    if (respin_count_ == 0) {
        filePath(fname, 0, path_, name_);
        Directory::eraseFile(fname.c_str());
    } else {
        uint32_t lastpos = respin_count_;
        while (lastpos >= 1) {
            filePath(fname, lastpos, path_, name_);
            if (FileDevice::size(fname.c_str()) >= 0) {
                break;
            }
            --lastpos;
        }
        string frompath;
        string topath;

        if (lastpos == respin_count_) {
            filePath(topath, respin_count_, path_, name_);
            --lastpos;
        } else {
            filePath(topath, lastpos + 1, path_, name_);
        }

        while (lastpos) {
            filePath(frompath, lastpos, path_, name_);
            Directory::renameFile(topath.c_str(), frompath.c_str());
            topath = frompath;
            --lastpos;
        }

        filePath(frompath, 0, path_, name_);
        Directory::renameFile(topath.c_str(), frompath.c_str());
        fname = frompath;
    }

    if (!fd.create(fname.c_str(), FileDevice::WriteOnlyE)) {
        cerr << "Cannot create log file: " << fname << endl;
        pos_         = &null_os_;
        respin_size_ = 0; //no more respins
    } else {
        if (pos_ == &basic_stream_) {
            basic_stream_.device(std::move(fd));
        } else if (pos_ == &stream_) {
            stream_.device(std::move(fd));
        } else {
            SOLID_ASSERT(false);
        }
    }
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
    tm                  loctm;
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
    return Engine::the().configure(_ros, _rmodule_mask_vec);
}

ErrorConditionT log_start(
    const char*                     _fname,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered,
    uint32_t                        _respincnt,
    uint64_t                        _respinsize)
{

    return Engine::the().configure(_fname, _rmodule_mask_vec, _buffered, _respincnt, _respinsize);
}

ErrorConditionT log_start(
    const char*                     _addr,
    const char*                     _port,
    const std::vector<std::string>& _rmodule_mask_vec,
    bool                            _buffered)
{
    return Engine::the().configure(_addr, _port, _rmodule_mask_vec, _buffered);
}

} //namespace solid
