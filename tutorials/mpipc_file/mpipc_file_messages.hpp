#ifndef TUTORIAL_IPC_REQUEST_MESSAGES_HPP
#define TUTORIAL_IPC_REQUEST_MESSAGES_HPP

#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <vector>
#include <deque>
#include <fstream>
#include <iostream>

namespace ipc_file{

struct ListRequest: solid::frame::mpipc::Message{
    std::string         path;

    ListRequest(){}

    ListRequest(std::string && _path): path(std::move(_path)){}

    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(path, "path");
    }
};

struct ListResponse: solid::frame::mpipc::Message{
    std::deque<std::pair<std::string, uint8_t> >    node_dq;

    ListResponse(){}

    ListResponse(const ListRequest &_rmsg):solid::frame::mpipc::Message(_rmsg){}

    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.pushContainer(node_dq, "node_dq");
    }
};


struct FileRequest: solid::frame::mpipc::Message{
    std::string         remote_path;
    std::string         local_path;


    FileRequest(){}

    FileRequest(
        std::string && _remote_path, std::string && _local_path
    ): remote_path(std::move(_remote_path)), local_path(std::move(_local_path)){}

    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(remote_path, "remote_path");
    }
};

struct FileResponse: solid::frame::mpipc::Message{
    std::string         remote_path;
    std::fstream        fs;
    int64_t             remote_file_size;

    FileResponse(){}

    FileResponse(
        const FileRequest &_rmsg
    ):  solid::frame::mpipc::Message(_rmsg), remote_path(_rmsg.remote_path),
        remote_file_size(solid::InvalidSize()){}

    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.pushCall(
            [this](S &_rs, solid::frame::mpipc::ConnectionContext &_rctx, uint64_t _val, solid::ErrorConditionT &_rerr){
                if(S::IsSerializer){
                    fs.open(remote_path.c_str());
                    _rs.pushStream(static_cast<std::istream*>(&fs), "fs");

                    if(fs){
                        std::streampos pos = fs.tellg();
                        fs.seekg(0, fs.end);
                        std::streampos endpos = fs.tellg();
                        fs.seekg(pos);
                        remote_file_size = endpos;
                    }else{
                        remote_file_size = solid::InvalidSize();
                    }
                    _rs.push(remote_file_size, "remote_file_size");
                }else{
                    std::string *plocal_path = localPath(_rctx);

                    if(remote_file_size != solid::InvalidSize() and plocal_path){
                        fs.open(plocal_path->c_str(), std::fstream::out | std::fstream::binary);
                    }
                    _rs.pushStream(static_cast<std::ostream*>(&fs), "fs");
                }
            }, 0, "reinit"
        );
        if(not S::IsSerializer){
            _s.push(remote_file_size, "remote_file_size");
        }
    }
private:
    std::string * localPath(solid::frame::mpipc::ConnectionContext &_rctx)const{
        auto req_ptr = std::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if(req_ptr){
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, ListRequest, ListResponse, FileRequest, FileResponse>;

}//namespace ipc_file

#endif


