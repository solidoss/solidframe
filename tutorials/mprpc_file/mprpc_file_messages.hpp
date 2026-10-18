
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
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

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("path", _rthis.path), _rctx);
    }
};

struct ListResponse : solid::frame::mprpc::Message {
    std::deque<std::pair<std::string, uint8_t>> node_dq;

    ListResponse() {}

    ListResponse(const ListRequest& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("nodes", _rthis.node_dq), _rctx);
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

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("remote_path", _rthis.remote_path), _rctx);
    }
};

struct FileResponse : solid::frame::mprpc::Message {
    std::string           remote_path;
    mutable int64_t       remote_file_size = 0;
    mutable std::ifstream ifs;
    std::ofstream         ofs;

    FileResponse() {}

    FileResponse(
        const FileRequest& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
        , remote_path(_rmsg.remote_path)
        , remote_file_size(solid::InvalidSize())
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(_rthis)>>) {

            _rthis.ifs.open(_rthis.remote_path);
            if (_rthis.ifs) {
                std::streampos pos = _rthis.ifs.tellg();
                _rthis.ifs.seekg(0, _rthis.ifs.end);
                std::streampos endpos = _rthis.ifs.tellg();
                _rthis.ifs.seekg(pos);
                _rthis.remote_file_size = endpos;
                _rr(id<1>, make("remote_file_size", _rthis.remote_file_size), _rctx);

                auto progress_lambda = [](std::istream& _ris, uint64_t _len, const bool _done, Context& _rctx) {
                    // NOTE: here you can use context.any()for actual implementation
                };
                _rr(id<2>, make("stream", _rthis.ifs).with_progress(progress_lambda), _rctx);

            } else {
                _rthis.remote_file_size = solid::InvalidSize();
                _rr(id<1>, make("remote_file_size", _rthis.remote_file_size), _rctx);
            }
        } else {
            _rr(id<1>, make("remote_file_size", _rthis.remote_file_size), _rctx);
            _rr(id<104>, make("callable", [&_rthis](auto&& _rr, Context& _rctx) -> void {
                auto progress_lambda = [](std::ostream& _ris, uint64_t _len, const bool _done, Context& _rctx) {
                    // NOTE: here you can use context.any()for actual implementation
                };

                if (_rthis.remote_file_size != solid::InvalidIndex()) {
                    const std::string* plocal_path = _rthis.localPath(_rctx);
                    if (plocal_path != nullptr) {
                        _rthis.ofs.open(*plocal_path);
                    }

                    _rr(id<2>, make("stream", _rthis.ofs).with_progress(progress_lambda), _rctx);
                }
            }),
                _rctx);
        }
    }

private:
    const std::string* localPath(solid::frame::mprpc::ConnectionContext& _rctx) const
    {
        auto req_ptr = solid::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if (req_ptr) {
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "ListRequest", std::type_identity<ListRequest>());
    _rreg(2, "ListResponse", std::type_identity<ListResponse>());
    _rreg(3, "FileRequest", std::type_identity<FileRequest>());
    _rreg(4, "FileResponse", std::type_identity<FileResponse>());
}

} // namespace rpc_file
