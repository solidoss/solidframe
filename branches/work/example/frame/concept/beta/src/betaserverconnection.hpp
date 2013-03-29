/* Declarations file betaserverconnection.hpp
	
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

#ifndef BETASERVERCONNECTION_HPP
#define BETASERVERCONNECTION_HPP

#include "utility/dynamictype.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "betaconnection.hpp"

namespace concept{

namespace beta{

namespace server{

class Command;

class Connection: public Dynamic<Connection, beta::Connection>{
public:
	typedef Service	ServiceT;
	
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	
	static Connection& the(){
		return static_cast<Connection&>(Object::the());
	}
	
	Connection(const SocketDevice &_rsd);
	
	~Connection();
	
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
	
	uint32 newRequestId(uint32 _pos = -1);
	bool   isRequestIdExpected(uint32 _v, uint32 &_rpos);
	void   deleteRequestId(uint32 _v);
	
	template <class S, uint32 I>
	int serializationReinit(S &_rs, const uint64 &_rv);
	
	
private:
	bool useEncryption()const;
	bool useCompression()const;
	
	void doPrepareRun();
	/*virtual*/ int	doFillSendBufferData(char *_sendbufpos);
	/*virtual*/ bool doParseBufferData(const char *_pbuf, ulong _len);
	void doCommandExecuteRecv(uint32 _cmdidx);
	bool doCommandExecuteSend(uint32 _cmdidx);
private:
	enum {
		Init,
		Run,
		SendException,
		SendExceptionDone
	};
	
	struct CommandStub{
		CommandStub(Command *_pcmd = NULL):pcmd(_pcmd){}
		Command *pcmd;
	};
	typedef std::vector<CommandStub>			CommandVectorT;
	typedef Queue<uint32>						UInt32QueueT;
	typedef Stack<uint32>						UInt32StackT;
	typedef std::pair<uint32, uint32>			UInt32PairT;
	typedef std::vector<UInt32PairT>			UInt32PairVectorT;
	
	uint32						reqid;
	DynamicExecuterT			de;
	CommandVectorT				cmdvec;
	UInt32QueueT				cmdque;
	uint16						crtcmdrecvtype;
	UInt32PairVectorT			reqvec;
};

}//namespace server

}//namespace beta

}//namespace concept

#endif
