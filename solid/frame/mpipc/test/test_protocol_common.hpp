#ifndef TEST_PROTOCOL_COMMON_HPP
#define TEST_PROTOCOL_COMMON_HPP

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

#include "solid/frame/mpipc/src/mpipcmessagereader.hpp"
#include "solid/frame/mpipc/src/mpipcmessagewriter.hpp"


namespace solid{namespace frame{namespace mpipc{

class TestEntryway{
public:
    static ConnectionContext& createContext(){
        Connection  &rcon = *static_cast<Connection*>(nullptr);
        Service     &rsvc = *static_cast<Service*>(nullptr);
        static ConnectionContext conctx(rsvc, rcon);
        return conctx;
    }
};

}/*namespace mpipc*/}/*namespace frame*/}/*namespace solid*/

#endif
