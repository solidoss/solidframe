/* Declarations file station.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UDPSTATION_H
#define UDPSTATION_H

#include <sys/poll.h>

#include "system/socketaddress.h"

#include "utility/queue.h"

namespace clientserver{
namespace udp{

class TalkerSelector;


class Station{
public:
	enum {RAISE_ON_DONE = 2};
	static Station *create();
	static Station *create(const AddrInfoIterator &_rai);
	static Station *create(const SockAddrPair &_rsa, AddrInfo::Family = AddrInfo::Inet4, int _proto = 0);
	~Station();
	int recvFrom(char *_pb, uint _bl, uint _flags = 0);
	int sendTo(const char *_pb, uint _bl, const SockAddrPair &_sap, uint _flags = 0);
	uint recvSize()const;
	const SockAddrPair &recvAddr() const;
	uint sendPendingCount() const;
	bool recvPending()const;
private:
	friend class TalkerSelector;
	struct Data{
		Data(char *_pb = NULL, uint _bl = 0, uint _flags = 0):bl(_bl),flags(0){
			b.pb = _pb;
		}
		Data(const SockAddrPair &_sap, 
			 char *_pb = NULL, 
			 uint _bl = 0, 
			 uint _flags = 0
		):bl(_bl), flags(0), sap(_sap){
			b.pb = _pb;
		}
		Data(const SockAddrPair &_sap, 
			 const char *_pb, 
			 uint _bl = 0, 
			 uint _flags = 0
		):bl(_bl), flags(0), sap(_sap){
			b.pcb = _pb;
		}
		
		union{
			char		*pb;
			const char	*pcb;
		} b;
		uint        	bl;
		uint        	flags;
		SockAddrPair	sap;
	};
	//typedef Queue<Data,8>			DataQueueTp;
	//TODO: use the same ideea as in tcp::Channel
	typedef Queue<Data,4>			DataQueueTp;
	enum {INTOUT = POLLIN, OUTTOUT = POLLOUT};
	Station(int sd);
	ulong ioRequest()const;
	int doSend();
	int doRecv();
	int descriptor() const;
private:
	int 			sd;
	uint			rcvsz;
	Data			rcvd;
	SocketAddress	rcvsa;
	DataQueueTp		sndq;
};

inline uint Station::recvSize()					const{return rcvsz;}
inline const SockAddrPair &Station::recvAddr()	const{return rcvd.sap;}
inline int Station::descriptor()				const{return sd;}
inline ulong Station::ioRequest()const{
	ulong rv = rcvd.b.pb ? INTOUT : 0;
	if(sndq.size()) rv |= OUTTOUT;
	return rv;
}
inline uint Station::sendPendingCount() const{
	return sndq.size();
}
inline bool Station::recvPending()const{
	return rcvd.b.pb;
}

}//namespace udp
}//namespace clientserver
#endif
