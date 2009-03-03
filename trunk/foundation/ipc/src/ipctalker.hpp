/* Declarations file ipctalker.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIENTSERVER_IPC_TALKER_HPP
#define CLIENTSERVER_IPC_TALKER_HPP

#include "foundation/aio/aiosingleobject.hpp"

struct TimeSpec;

namespace foundation{

class Visitor;

namespace ipc{

class Service;
struct Buffer;
struct ConnectorUid;

//! A talker for io requests
class Talker: public foundation::aio::SingleObject{
public:
	typedef Service							ServiceTp;
	typedef foundation::aio::SingleObject	BaseTp;
	
	Talker(const SocketDevice &_rsd, Service &_rservice, uint16 _id);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(foundation::Visitor &);
	int pushCommand(foundation::CmdPtr<Command> &_pcmd, const ConnectorUid &_rconid, uint32 _flags);
	void pushProcessConnector(ProcessConnector *_pc, ConnectorUid &_rconid, bool _exists = false);
	void disconnectProcesses();
private:
	bool processCommands(const TimeSpec &_rts);
	bool dispatchSentBuffer(const TimeSpec &_rts);
	void dispatchReceivedBuffer(const SockAddrPair &_rsap, const TimeSpec &_rts);
	void optimizeBuffer(Buffer &_rbuf);
	bool inDone(ulong _sig, const TimeSpec &_rts);
	struct Data;
	Data &d;
};

}//namespace ipc
}//namespace foundation

#endif

