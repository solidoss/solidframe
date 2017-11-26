
#pragma once

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

#include "solid/frame/mpipc/src/mpipcmessagereader.hpp"
#include "solid/frame/mpipc/src/mpipcmessagewriter.hpp"

namespace solid {
namespace frame {
namespace mpipc {

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

} /*namespace mpipc*/
} /*namespace frame*/
} /*namespace solid*/
