#ifndef TEST_PROTOCOL_COMMON_HPP
#define TEST_PROTOCOL_COMMON_HPP

#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcprotocol_serialization_v1.hpp"

#include "frame/ipc/src/ipcmessagereader.hpp"
#include "frame/ipc/src/ipcmessagewriter.hpp"


namespace solid{namespace frame{namespace ipc{

class TestEntryway{
public:
	static ConnectionContext& createContext(){
		Connection	&rcon = *static_cast<Connection*>(nullptr);
		Service		&rsvc = *static_cast<Service*>(nullptr);
		static ConnectionContext conctx(rsvc, rcon);
		return conctx;
	}
};

}/*namespace ipc*/}/*namespace frame*/}/*namespace solid*/

#endif
