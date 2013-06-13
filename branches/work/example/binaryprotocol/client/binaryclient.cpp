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

int main(int argc, char *argv[]){
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		
		frame::Manager				m;
		AioSchedulerT				aiosched(m);
		ClientConnectionPointerT	ccptr(new ClientConnection(m));
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<protocol::binary::Message>	msgptr(new TestRequest);
		
		ccptr->sendRequest(msgptr, TestCallback(*ccptr));
		
		wait();
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}