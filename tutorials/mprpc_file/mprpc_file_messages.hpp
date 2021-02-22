
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <vector>

namespace rpc_file {

struct ListRequest : solid::frame::mprpc::Message {
    std::string path;

    ListRequest() {}

    ListRequest(std::string&& _path)
        : path(std::move(_path))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.path, _rctx, 1, "path");
    }
};

struct ListResponse : solid::frame::mprpc::Message {
    std::deque<std::pair<std::string, uint8_t>> node_dq;

    ListResponse() {}

    ListResponse(const ListRequest& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.node_dq, _rctx, 1, "nodes");
    }
};

struct FileRequest : solid::frame::mprpc::Message {
    std::string remote_path;
    std::string local_path;

    FileRequest() {}

    FileRequest(
        std::string&& _remote_path, std::string&& _local_path)
        : remote_path(std::move(_remote_path))
        , local_path(std::move(_local_path))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.remote_path, _rctx, 1, "remote_path");
    }
};

struct FileResponse : solid::frame::mprpc::Message {
    std::string remote_path;
    mutable int64_t     remote_file_size;
    mutable std::ifstream ifs;
    std::ofstream ofs;

    FileResponse() {}

    FileResponse(
        const FileRequest& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
        , remote_path(_rmsg.remote_path)
        , remote_file_size(solid::InvalidSize())
    {
    }
    
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        if constexpr( Reflector::is_const_reflector){
            
            _rthis.ifs.open(_rthis.remote_path);
            if (_rthis.ifs) {
                std::streampos pos = _rthis.ifs.tellg();
                _rthis.ifs.seekg(0, _rthis.ifs.end);
                std::streampos endpos = _rthis.ifs.tellg();
                _rthis.ifs.seekg(pos);
                _rthis.remote_file_size = endpos;
                _rr.add(_rthis.remote_file_size, _rctx, 1, "remote_file_size");
                
                auto progress_lambda = [](Context &_rctx, std::istream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name) {
                    //NOTE: here you can use context.anyTuple for actual implementation
                };
                _rr.add(_rthis.ifs, _rctx, 2, "stream", [&progress_lambda](auto& _rmeta){_rmeta.progressFunction(progress_lambda);});
                
            } else {
                _rthis.remote_file_size = solid::InvalidSize();
                _rr.add(_rthis.remote_file_size, _rctx, 1, "remote_file_size");
            }
        }else{
            _rr.add(_rthis.remote_file_size, _rctx, 1, "remote_file_size");
            _rr.add(
                [&_rthis](Reflector &_rr, Context &_rctx){
                    auto progress_lambda = [](Context &_rctx, std::ostream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name) {
                        //NOTE: here you can use context.anyTuple for actual implementation
                    };
                    
                    if (_rthis.remote_file_size != solid::InvalidIndex()) {
                        const std::string* plocal_path = _rthis.localPath(_rctx);
                        if (plocal_path != nullptr) {
                            _rthis.ofs.open(*plocal_path);
                        }
                    
                        _rr.add(_rthis.ofs, _rctx, 2, "stream", [&progress_lambda](auto& _rmeta){_rmeta.progressFunction(progress_lambda);});
                    }
                }, _rctx
            );
            
        }
    }

private:
    const std::string* localPath(solid::frame::mprpc::ConnectionContext& _rctx) const
    {
        auto req_ptr = std::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if (req_ptr) {
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "ListRequest", solid::TypeToType<ListRequest>());
    _rreg(2, "ListResponse", solid::TypeToType<ListResponse>());
    _rreg(3, "FileRequest", solid::TypeToType<FileRequest>());
    _rreg(4, "FileResponse", solid::TypeToType<FileResponse>());
}

} //namespace rpc_file
