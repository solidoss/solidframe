
#pragma once

#include <cstddef>

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"

#include "solid/frame/mprpc/src/mprpcmessagereader.hpp"
#include "solid/frame/mprpc/src/mprpcmessagewriter.hpp"

namespace solid::frame::mprpc {

class TestEntryway {
public:
    static ConnectionContext& createContext()
    {
        Connection&                 rcon = *createConnection();
        Service&                    rsvc = *createService();
        frame::aio::ReactorContext& rctx = *createReactorContex();
        static ConnectionContext    conctx(rctx, rsvc, rcon);
        return conctx;
    }

private:
    // The protocol tests never touch these objects, they only need valid
    // addresses to bind the ConnectionContext references to.
    template <class T>
    static T* placeholder()
    {
        alignas(std::max_align_t) static std::byte storage[1024] = {};
        return reinterpret_cast<T*>(storage);
    }
    static Connection* createConnection()
    {
        return placeholder<Connection>();
    }
    static Service* createService()
    {
        return placeholder<Service>();
    }
    static frame::aio::ReactorContext* createReactorContex()
    {
        return placeholder<frame::aio::ReactorContext>();
    }
};

} // namespace solid::frame::mprpc
