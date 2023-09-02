// solid/frame/file/filestore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/file/tempbase.hpp"
#include "solid/frame/sharedstore.hpp"
#include "solid/system/filedevice.hpp"
#include "solid/system/log.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {

class Manager;

namespace file {

extern const LoggerT logger;

enum {
    MemoryLevelFlag   = 1,
    VeryFastLevelFlag = 2,
    FastLevelFlag     = 4,
    NormalLevelFlag   = 8,
    SlowLevelFlag     = 16,
    AllLevelsFlag     = 1 + 2 + 4 + 8 + 16,
};

enum TempRemoveMode {
    RemoveAfterCreateE,
    RemoveAfterCloseE,
    RemoveNeverE
};

struct TempConfiguration {
    struct Storage {
        Storage()
            : level(0)
            , capacity(0)
            , minsize(0)
            , maxsize(0)
            , removemode(RemoveAfterCreateE)
        {
        }

        std::string    path;
        uint32_t       level;
        uint64_t       capacity;
        uint64_t       minsize;
        uint64_t       maxsize;
        TempRemoveMode removemode;
    };

    typedef std::vector<Storage> StorageVectorT;

    StorageVectorT storagevec;
};

struct Utf8Controller;

struct File {
    File()
        : ptmp(nullptr)
    {
    }
    ~File()
    {
        clear();
    }
    void clear()
    {
        fd.close();
        delete ptmp;
        ptmp = nullptr;
    }
    bool open(const char* _path, const int _openflags)
    {
        return fd.open(_path, _openflags);
    }

    // We only offer offset based io because in case of shared use
    // the file offset will be kept by streams

    ssize_t read(char* _pb, size_t _bl, int64_t _off)
    {
        if (!ptmp) {
            return fd.read(_pb, _bl, _off);
        } else {
            return ptmp->read(_pb, _bl, _off);
        }
    }
    ssize_t write(const char* _pb, size_t _bl, int64_t _off)
    {
        if (!ptmp) {
            return fd.write(_pb, _bl, _off);
        } else {
            return ptmp->write(_pb, _bl, _off);
        }
    }
    int64_t size() const
    {
        if (!ptmp) {
            return fd.size();
        } else {
            return ptmp->size();
        }
    }
    bool truncate(int64_t _len = 0)
    {
        if (!ptmp) {
            return fd.truncate(_len);
        } else {
            return ptmp->truncate(_len);
        }
    }
    int64_t capacity() const
    {
        if (!ptmp) {
            return -1;
        } else {
            return ptmp->tempsize;
        }
    }
    void flush()
    {
        if (!ptmp) {
            if (fd) {
                fd.flush();
            }
        } else {
            ptmp->flush();
        }
    }
    bool isTemp() const
    {
        return ptmp != nullptr;
    }
    TempBase* temp() const
    {
        return ptmp;
    }

private:
    friend struct Utf8Controller;
    FileDevice fd;
    TempBase*  ptmp;
};

typedef shared::Pointer<File> FilePointerT;

template <class S, class Cmd, class Base>
struct OpenCommand : Base {
    Cmd cmd;

    OpenCommand(
        Cmd const& _cmd, typename Base::PathT _path, size_t _openflags)
        : Base(_path, _openflags)
        , cmd(_cmd)
    {
    }

    void operator()(S& _rstore, FilePointerT& _rptr, ErrorCodeT _err)
    {
        if (!_err) {
            Base::openFile(_rstore, _rptr, _err);
        }
        cmd(_rstore, _rptr, _err);
    }
};

struct CreateTempCommandBase {
    size_t   openflags;
    uint64_t size;
    size_t   storageid;

protected:
    CreateTempCommandBase(
        uint64_t _sz, size_t _openflags)
        : openflags(_openflags)
        , size(_sz)
        , storageid(InvalidIndex())
    {
    }

    void openTemp(Utf8Controller& _rstore, FilePointerT& _rptr, ErrorCodeT& _rerr);
};

template <class S, class Cmd>
struct CreateTempCommand : CreateTempCommandBase {
    Cmd cmd;

    CreateTempCommand(
        Cmd const& _cmd, uint64_t _sz, size_t _createflags)
        : CreateTempCommandBase(_sz, _createflags)
        , cmd(_cmd)
    {
    }

    void operator()(S& _rstore, FilePointerT& _rptr, ErrorCodeT _err)
    {
        CreateTempCommandBase::openTemp(_rstore, _rptr, _err);
        cmd(_rstore, _rptr, _err);
    }
};

struct Utf8Configuration {
    struct Storage {
        Storage() {}
        Storage(
            std::string const& _globalprefix,
            std::string const& _localprefix)
            : globalprefix(_globalprefix)
            , localprefix(_localprefix)
        {
        }
        std::string globalprefix;
        std::string localprefix;
    };
    typedef std::vector<Storage> StorageVectorT;

    StorageVectorT storagevec;
};

struct Utf8OpenCommandBase;

struct Utf8Controller {
    typedef Utf8OpenCommandBase OpenCommandBaseT;

    Utf8Controller(const Utf8Configuration& _rfilecfg, const TempConfiguration& _rtempcfg);
    ~Utf8Controller();

    // NOTE: it would have been nice to have all the below methods as protected
    bool clear(shared::StoreBase::Accessor& _rsbacc, File& _rf, const size_t _idx);
    bool executeBeforeErase(shared::StoreBase::Accessor& _rsbacc);
    void executeOnSignal(shared::StoreBase::Accessor& _rsbacc, ulong _sm);

    bool prepareIndex(
        shared::StoreBase::Accessor& _rsbacc, Utf8OpenCommandBase& _rcmd,
        size_t& _ridx, size_t& _rflags, ErrorCodeT& _rerr);

    bool preparePointer(
        shared::StoreBase::Accessor& _rsbacc, Utf8OpenCommandBase& _rcmd,
        FilePointerT& _rptr, size_t& _rflags, ErrorCodeT& _rerr);

    bool prepareIndex(
        shared::StoreBase::Accessor& _rsbacc, CreateTempCommandBase& _rcmd,
        size_t& _ridx, size_t& _rflags, ErrorCodeT& _rerr);
    bool preparePointer(
        shared::StoreBase::Accessor& _rsbacc, CreateTempCommandBase& _rcmd,
        FilePointerT& _rptr, size_t& _rflags, ErrorCodeT& _rerr);

private:
    friend struct CreateTempCommandBase;
    friend struct Utf8OpenCommandBase;
    void openFile(Utf8OpenCommandBase& _rcmd, FilePointerT& _rptr, ErrorCodeT& _rerr);
    void openTemp(CreateTempCommandBase& _rcmd, FilePointerT& _rptr, ErrorCodeT& _rerr);
    void doPrepareOpenTemp(File& _rf, uint64_t _sz, const size_t _storeid);
    void doCloseTemp(TempBase& _rtemp);
    void doDeliverTemp(shared::StoreBase::Accessor& _rsbacc, const size_t _storeid);

private:
    struct Data;
    Pimpl<Data, 504> impl_;
};

struct Utf8PathStub {
    size_t      storeidx;
    std::string path;
    size_t      idx;
};

struct Utf8OpenCommandBase {
    typedef std::string const& PathT;

    Utf8PathStub       outpath;
    std::string const& inpath;
    size_t             openflags;

    Utf8OpenCommandBase(
        std::string const& _inpath, size_t _openflags)
        : inpath(_inpath)
        , openflags(_openflags)
    {
    }

    void openFile(Utf8Controller& _rstore, FilePointerT& _rptr, ErrorCodeT& _rerr);
};

template <class Base = Utf8Controller>
class Store : public shared::Store<File, Store<Base>>, public Base {

    typedef Base                             BaseT;
    typedef shared::Store<File, Store<Base>> StoreT;
    typedef Store<Base>                      ThisT;

public:
    template <typename C>
    Store(
        Manager& _rm,
        C        _c)
        : StoreT(_rm)
        , BaseT(_c)
    {
    }

    template <typename C1, typename C2>
    Store(
        Manager& _rm,
        C1 _c1, C2 _c2)
        : StoreT(_rm)
        , BaseT(_c1, _c2)
    {
    }

    // If a file with _path already exists in the store, the call will be similar with open with truncate openflag
    template <class Cmd>
    bool requestCreateFile(Cmd const& _cmd, std::string const& _path, const size_t _openflags = 0, const size_t _flags = 0)
    {
        OpenCommand<ThisT, Cmd,
            typename BaseT::OpenCommandBaseT>
            cmd(_cmd, _path, _openflags | FileDevice::CreateE | FileDevice::TruncateE);
        return StoreT::requestReinit(cmd, _flags);
    }

    template <class Cmd>
    bool requestOpenFile(Cmd const& _cmd, std::string const& _path, const size_t _openflags = 0, const size_t _flags = 0)
    {
        OpenCommand<ThisT, Cmd,
            typename BaseT::OpenCommandBaseT>
            cmd(_cmd, _path, _openflags);
        return StoreT::requestReinit(cmd, _flags);
    }

    template <class Cmd>
    bool requestCreateTemp(
        Cmd const& _cmd, uint64_t _sz, const size_t _createflags = AllLevelsFlag, const size_t _flags = 0)
    {
        CreateTempCommand<ThisT, Cmd> cmd(_cmd, _sz, _createflags);
        return StoreT::requestReinit(cmd, _flags);
    }
};

inline void Utf8OpenCommandBase::openFile(Utf8Controller& _rstore, FilePointerT& _rptr, ErrorCodeT& _rerr)
{
    _rstore.openFile(*this, _rptr, _rerr);
}

inline void CreateTempCommandBase::openTemp(Utf8Controller& _rstore, FilePointerT& _rptr, ErrorCodeT& _rerr)
{
    _rstore.openTemp(*this, _rptr, _rerr);
}

} // namespace file
} // namespace frame
} // namespace solid
