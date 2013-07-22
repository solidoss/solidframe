#include "protocol/binary/client/clientsession.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "system/thread.hpp"
#include "example/binaryprotocol/core/messages.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

class ClientConnection: public frame::aio::SingleObject{
	typedef std::vector<DynamicPointer<solid::frame::Message> > MessageVectorT;
	enum{
		RecvBufferCapacity = 1024 * 4,
		SendBufferCapacity = 1024 * 4,
	};
public:
	ClientConnection(frame::Manager &_rm):rm(_rm){}
	
	/*virtual*/ int execute(ulong _evs, TimeSpec& _crtime);
	
	void send(DynamicPointer<solid::frame::Message>	&_rmsgptr){
		Locker<Mutex>	lock(rm.mutex(*this));
		
		sndmsgvec.push_back(_rmsgptr);
		
		if(Object::notify(frame::S_SIG | frame::S_RAISE)){
			rm.raise(*this);
		}
	}
private:
	frame::Manager						&rm;
	protocol::binary::client::Session	protoses;
	MessageVectorT						sndmsgvec;
	char								recvbuf[RecvBufferCapacity];
	char								sendbuf[SendBufferCapacity];
};

/*virtual*/ int ClientConnection::execute(ulong _evs, TimeSpec& _crtime){
	ulong sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL) return BAD;
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(rm.mutex(*this));
			protoses.schedule(sndmsgvec.begin(), sndmsgvec.end());
			sndmsgvec.clear();
		}
	}
	if(_evs & frame::INDONE){
		if(!protoses.consume(recvbuf, this->socketRecvSize())){
			return BAD;
		}
	}
	bool reenter = false;
	if(!this->socketHasPendingRecv()){
		switch(this->socketRecv(recvbuf, RecvBufferCapacity)){
			case BAD: return BAD;
			case OK:
				if(!protoses.consume(recvbuf, this->socketRecvSize())){
					return BAD;
				}
				reenter = true;
				break;
// 			case NOK:
// 				break;
			default:
				break;
		}
	}
	if(!this->socketHasPendingSend()){
		int cnt = 4;
		while((cnt--) > 0){
			int rv = protoses.fill(sendbuf, SendBufferCapacity);
			if(rv == BAD) return BAD;
			if(rv == NOK) break;
			switch(this->socketSend(sendbuf, rv)){
				case BAD: 
					return BAD;
// 				case OK:
// 					break;
				case NOK:
					cnt = 0;
					break;
				default:
					break;
			}
		}
		if(cnt == 0){
			reenter = true;
		}
	}
	return reenter ? OK : NOK;
}

int main(int argc, char *argv[]){
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		ClientConnectionPointerT				ccptr(new ClientConnection(m));
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<solid::frame::Message>	msgptr;
		
		msgptr = new FirstRequest;
		
		ccptr->send(msgptr);
		
		msgptr = new SecondRequest;
		
		ccptr->send(msgptr);
		
		wait();
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}