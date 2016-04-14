#ifndef TEST_PROTOCOL_COMMON_HPP
#define TEST_PROTOCOL_COMMON_HPP

#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcserialization.hpp"

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
	
	template <class Msg, class CompleteCbk>
	static void initTypeMap(
		frame::ipc::TypeIdMapT &_rtm,
		CompleteCbk _completecbk
	){
		TypeStub ts;
		ts.complete_fnc = MessageCompleteFunctionT(_completecbk);
		
		_rtm.registerType<Msg>(
			ts,
			Message::serialize<SerializerT, Msg>,
			Message::serialize<DeserializerT, Msg>,
			serialization::basic_factory<Msg>
		);
		_rtm.registerCast<Msg, frame::ipc::Message>();
	}
};

}/*namespace ipc*/}/*namespace frame*/}/*namespace solid*/

#endif
