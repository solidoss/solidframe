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

struct TestCallback{
	ClientConnection &rcc;
	
	TestCallback(ClientConnection &_rcc):rcc(_rcc){}
	
	void operator()(auto_ptr<TestResponse> &_rresptr){
	
	}
};


struct Callback{
	ClientConnection &rcc;
	
	Callback(ClientConnection &_rcc):rcc(_rcc){}
	
	
	void receive(auto_ptr<FirstResponse>	&_rres);
	void receive(auto_ptr<SecondResponse>	&_rres);
	void complete(int _err);
};

int main(int argc, char *argv[]){
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		
		frame::Manager				m;
		AioSchedulerT				aiosched(m);
		ClientConnectionPointerT	ccptr(new ClientConnection(m));
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<protocol::binary::Message>	msgptr(new FirstRequest);
		
		ccptr->sendRequest(msgptr, Callback(*ccptr));
		
		wait();
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}