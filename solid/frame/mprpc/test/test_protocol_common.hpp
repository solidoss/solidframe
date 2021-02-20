
#pragma once

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"

#include "solid/frame/mprpc/src/mprpcmessagereader.hpp"
#include "solid/frame/mprpc/src/mprpcmessagewriter.hpp"

namespace solid {
namespace frame {
namespace mprpc {

class TestEntryway {
public:
    static ConnectionContext& createContext()
    {
        Connection&              rcon = *createConnection();
        Service&                 rsvc = *createService();
        static ConnectionContext conctx(rsvc, rcon);
        return conctx;
    }
    static Connection* createConnection()
    {
        return nullptr;
    }
    static Service* createService()
    {
        return nullptr;
    }
};

} /*namespace mprpc*/
} /*namespace frame*/
} /*namespace solid*/
