
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <vector>

namespace ipc_file {

struct ListRequest : solid::frame::mpipc::Message {
    std::string path;

    ListRequest() {}

    ListRequest(std::string&& _path)
        : path(std::move(_path))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.path, _rctx, "path");
    }
};

struct ListResponse : solid::frame::mpipc::Message {
    std::deque<std::pair<std::string, uint8_t>> node_dq;

    ListResponse() {}

    ListResponse(const ListRequest& _rmsg)
        : solid::frame::mpipc::Message(_rmsg)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.node_dq, _rctx, "nodes");
    }
};

struct FileRequest : solid::frame::mpipc::Message {
    std::string remote_path;
    std::string local_path;

    FileRequest() {}

    FileRequest(
        std::string&& _remote_path, std::string&& _local_path)
        : remote_path(std::move(_remote_path))
        , local_path(std::move(_local_path))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.remote_path, _rctx, "remote_path");
    }
};

struct FileResponse : solid::frame::mpipc::Message {
    std::string remote_path;
    int64_t     remote_file_size;

    FileResponse() {}

    FileResponse(
        const FileRequest& _rmsg)
        : solid::frame::mpipc::Message(_rmsg)
        , remote_path(_rmsg.remote_path)
        , remote_file_size(solid::InvalidSize())
    {
    }

    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) const
    {
        std::ifstream ifs;
        ifs.open(remote_path);
        if (ifs) {
            std::streampos pos = ifs.tellg();
            ifs.seekg(0, ifs.end);
            std::streampos endpos = ifs.tellg();
            ifs.seekg(pos);
            const int64_t file_size = endpos;
            _s.add(file_size, _rctx, "remote_file_size");

            _s.push([ifs = std::move(ifs)](S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) mutable {
                _s.add(ifs, [](std::istream& _ris, uint64_t _len, const bool _done, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {
                    //idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                },
                    _rctx, _name);
                return true;
            },
                _rctx, _name);
        } else {
            const int64_t file_size = solid::InvalidSize();
            _s.add(file_size, _rctx, "remote_file_size");
        }
    }
    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        _s.add(remote_file_size, _rctx, "remote_file_size");
        _s.add([this](S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {
            if (remote_file_size != solid::InvalidIndex()) {
                std::ofstream      ofs;
                const std::string* plocal_path = localPath(_rctx);
                if (plocal_path != nullptr) {
                    ofs.open(*plocal_path);
                }
                _s.push([this, ofs = std::move(ofs)](S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) mutable {
                    _s.add(ofs, [this](std::ostream& _ros, uint64_t _len, const bool _done, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {
                        //idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                    },
                        _rctx, _name);
                    return true;
                },
                    _rctx, _name);
            }
        },
            _rctx, _name);
    }

private:
    const std::string* localPath(solid::frame::mpipc::ConnectionContext& _rctx) const
    {
        auto req_ptr = std::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if (req_ptr) {
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};

using ProtocolT = solid::frame::mpipc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<ListRequest>(), 1);
    _r(_rproto, solid::TypeToType<ListResponse>(), 2);
    _r(_rproto, solid::TypeToType<FileRequest>(), 3);
    _r(_rproto, solid::TypeToType<FileResponse>(), 4);
}

} //namespace ipc_file
