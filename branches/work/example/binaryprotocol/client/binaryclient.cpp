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

struct FirstRequest{
	
};

struct SecondRequest{
	
};

struct FirstResponse{
	
};

struct SecondResponse{
	
};

struct FirstHandler{
	ClientConnection &rcc;
	
	FirstHandler(ClientConnection &_rcc):rcc(_rcc){}
	
	
	void handle(auto_ptr<FirstResponse>	&_rres);
	
	void handle(int _err);
};

struct SecondHandler{
	ClientConnection &rcc;
	
	SecondHandler(ClientConnection &_rcc):rcc(_rcc){}
	
	
	void handle(auto_ptr<FirstResponse>		&_rres);
	void handle(auto_ptr<SecondResponse>	&_rres);
	
	void handle(int _err);
};


int main(int argc, char *argv[]){
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		typedef serialization::IdTypeMapper<BinSerializer, BinDeserializer, uint16>		UInt16TypeMapper;
		
		frame::Manager				m;
		AioSchedulerT				aiosched(m);
		UInt16TypeMapper			tm;
		ClientConnectionPointerT	ccptr(new ClientConnection(m, tm));
		
		
		tm.insert<FirstRequest>();
		tm.insert<SecondRequest>();
		tm.insert<FirstResponse>();
		tm.insert<SecondResponse>();
		
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