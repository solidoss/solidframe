#include "protocol/binary/client/clientsession.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/manager.hpp"
#include "system/thread.hpp"

#include <iostream>

using namespace solid;
using namespace std;

class ClientConnection: frame::aio::SingleObject, protocol::binary::client::Session{
public:
private:
};


struct ConnectionContext{
	ClientConnection &rcon;
};

struct FirstRequest{
	
};

struct SecondRequest{
	
};

struct FirstResponse{
	
};

struct SecondResponse{
	
};

struct FirstHandler{
	ConnectionContext &rctx;
	
	FirstHandler(ConnectionContext &_rctx):rctx(_rctx){}
	
	bool checkStore(){
		//returning false will generate a serialization error
		return true;
	}
	
	bool checkLoad(){
		//here we should check if the response is received in the right state
		//returning false will generate a deserialization error
		return true;
	}
	void handle(auto_ptr<FirstResponse>	&_rmsg){
		
	}
};

struct SecondHandler{
	ConnectionContext &rctx;
	
	SecondHandler(ConnectionContext &_rctx):rctx(_rctx){}
	
	void handle(auto_ptr<SecondResponse>	&_rmsg);
};


int main(int argc, char *argv[]){
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		typedef serialization::IdTypeMapper<
			BinSerializer,
			BinDeserializer,
			uint16,
			ConnectionContext
		>												UInt16TypeMapper;
		
		frame::Manager				m;
		AioSchedulerT				aiosched(m);
		UInt16TypeMapper			tm;
		ClientConnectionPointerT	ccptr(new ClientConnection(m, tm));
		
		
		tm.insert<FirstRequest>();
		tm.insert<SecondRequest, FirstHandler>();
		tm.insert<FirstResponse>();
		tm.insert<SecondResponse, SecondHandler>();
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<protocol::binary::Message>	msgptr;
		msgptr = new FirstRequest;
		
		ccptr->sendRequest(msgptr, FirstHandler(*ccptr));
		
		msgptr = new SecondRequest;
		
		ccptr->sendRequest(msgptr, SecondHandler(*ccptr));
		
		wait();
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}