/* Declarations file betaservercommand.hpp
	
	Copyright 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BETACLIENTCONNECTION_HPP
#define BETACLIENTCONNECTION_HPP


#include <vector>
#include "utility/queue.hpp"
#include "utility/stack.hpp"
#include "betaconnection.hpp"


namespace foundation{
class Visitor;
}

namespace concept{

class Manager;

namespace beta{

struct LoginSignal;
struct CancelSignal;

namespace client{

class Command;

class Connection: public Dynamic<Connection, beta::Connection>{
public:
	typedef Service	ServiceT;
	
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	
	static Connection& the(){
		return static_cast<Connection&>(Object::the());
	}
	
	Connection(const SocketAddressInfo &_raddrinfo);
	
	~Connection();
	
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
	
	uint32 requestId()const{return reqid;}
	uint32 newRequestId(){
		if(++reqid) return reqid;
		return (reqid = 1);
	}
	
	template <class S, uint32 I>
	int serializationReinit(S &_rs, const uint64 &_rv);
	void pushCommand(Command *_pcmd);
	uint32 commandUid(const uint32 _cmdidx)const;
	
	void dynamicExecute(DynamicPointer<LoginSignal> &_rsig);
	void dynamicExecute(DynamicPointer<CancelSignal> &_rsig);
private:
	bool useEncryption()const;
	bool useCompression()const;
	
	void doPrepareRun();
	/*virtual*/ int	doFillSendBufferData(char *_sendbufpos);
	/*virtual*/ bool doParseBufferData(const char *_pbuf, ulong _len);
	/*virtual*/ int doParseBufferException(const char *_pbuf, ulong _len);
private:
	enum {
		Init,
		ConnectPrepare,
		ConnectNext,
		Connect,
		ConnectWait,
		Run
	};
	
	struct CommandStub{
		CommandStub(Command *_pcmd = NULL, uint32 _uid = 0):pcmd(_pcmd), uid(_uid), sendtype(true){}
		Command *pcmd;
		uint32 	uid;
		bool	sendtype;
	};
	typedef std::vector<CommandStub>			CommandVectorT;
	typedef Queue<uint32>						UInt32QueueT;
	typedef Stack<uint32>						UInt32StackT;
	
	SocketAddressInfo			addrinfo;
	SocketAddressInfoIterator	addrit;
	uint32						reqid;
	DynamicExecuterT			de;
	CommandVectorT				cmdvec;
	UInt32QueueT				cmdque;
	UInt32StackT				cmdvecfreestk;
	uint16						crtcmdsendtype;
};

inline uint32 Connection::commandUid(const uint32 _cmdidx)const{
	return cmdvec[_cmdidx].uid;
}


}//namespace client

}//namespace beta

}//namespace concept

#endif
