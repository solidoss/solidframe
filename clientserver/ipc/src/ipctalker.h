/* Declarations file ipctalker.h
	
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

#ifndef CLUSTER_TALKER_H
#define CLUSTER_TALKER_H

#include "udp/talker.h"
//#include "clientserver/core/commandableobject.h"

struct TimeSpec;

namespace serialization{
namespace bin{
class RTTIMapper;
}
}

namespace clientserver{

class Visitor;

namespace ipc{

typedef serialization::bin::RTTIMapper BinMapper;
class Service;
struct Buffer;
struct ConnectorUid;

//! A talker for io requests
class Talker: public clientserver::udp::Talker{
public:
	typedef Service						ServiceTp;
	typedef clientserver::udp::Talker	BaseTp;
	
	Talker(clientserver::udp::Station *_pst, Service &_rservice, BinMapper &_rmapper, uint16 _id);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(clientserver::Visitor &);
	int pushCommand(clientserver::CmdPtr<Command> &_pcmd, const ConnectorUid &_rconid, uint32 _flags);
	void pushProcessConnector(ProcessConnector *_pc, ConnectorUid &_rconid, bool _exists = false);
	void disconnectProcesses();
private:
	bool processCommands(const TimeSpec &_rts);
	bool dispatchSentBuffer(const TimeSpec &_rts);
	void dispatchReceivedBuffer(const SockAddrPair &_rsap);
	void optimizeBuffer(Buffer &_rbuf);
	bool inDone(ulong _sig);
	struct Data;
	Data &d;
};

}//namespace ipc
}//namespace clientserver

#endif

