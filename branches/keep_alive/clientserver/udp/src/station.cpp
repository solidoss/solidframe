/* Implementation file station.cpp
	
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

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/socketaddress.hpp"

#include "core/common.hpp"
#include "udp/station.hpp"
#include <new>

namespace clientserver{
namespace udp{
#ifndef UINLINES
#include "station.ipp"
#endif
//-----------------------------------------------------------------------------------------
// Station* Station::create(){
// 	SocketDevice sd;
// 	sd.create(AddrInfo::Inet4, AddrInfo::Datagram, 0);
// 	sd.makeNonBlocking();
// 	if(sd.ok())	return new Station(sd);
// 	return NULL;
// }
//-----------------------------------------------------------------------------------------
Station* Station::create(const AddrInfoIterator &_rai){
	SocketDevice sd;
	sd.create(_rai);
	sd.bind(_rai);
	sd.makeNonBlocking();
	if(sd.ok()) return new Station(sd);
	return NULL;
}
//-----------------------------------------------------------------------------------------
Station *Station::create(const SockAddrPair &_rsa, AddrInfo::Family _fam, int _proto){
	SocketDevice sd;
	sd.create(_fam, AddrInfo::Datagram, _proto);
	sd.makeNonBlocking();
	sd.bind(_rsa);
	if(sd.ok())	return new Station(sd);
	return NULL;
}
//-----------------------------------------------------------------------------------------
Station::Station(SocketDevice &_sd):sd(_sd),rcvsz(0){
}
//-----------------------------------------------------------------------------------------
Station::~Station(){
}
//-----------------------------------------------------------------------------------------
int Station::recvFrom(char *_pb, unsigned _bl, unsigned _flags){
	if(!_bl) return OK;
	rcvsa.size() = SocketAddress::MaxSockAddrSz;
	ssize_t rv = sd.recv(_pb, _bl, rcvsa);
	if(rv > 0){
		rcvsz = rv;
		rcvd.sap.addr = rcvsa.addr();
		rcvd.sap.size = rcvsa.size();
		return OK;
	}
	if(rv == 0) return BAD;
	if(rv < 0 && errno == EAGAIN){
		rcvd.b.pb = _pb;
		rcvd.bl = _bl;
		return NOK;
	}
	return BAD;
}
//-----------------------------------------------------------------------------------------
int Station::sendTo(const char *_pb, unsigned _bl, const SockAddrPair &_sap, unsigned _flags){
	if(!_bl) return OK;
	if(sndq.empty()){
		ssize_t rv = sd.send(_pb, _bl, _sap);
		if(rv == (ssize_t)_bl) return OK;
		if(rv >= 0) return BAD;
		if(rv < 0 && errno != EAGAIN) return BAD;
	}
	sndq.push(Data(_sap, _pb, _bl, _flags));
	return NOK;
}
//-----------------------------------------------------------------------------------------
int Station::doRecv(){
	if(rcvd.b.pb){
		rcvsa.size() = SocketAddress::MaxSockAddrSz;
		ssize_t rv = sd.recv(rcvd.b.pb, rcvd.bl, rcvsa);
		if(rv > 0) {
			rcvd.b.pb = NULL; rcvsz = rv; 
			rcvd.sap.addr = rcvsa.addr();
			rcvd.sap.size = rcvsa.size();
			return INDONE;
		}
		if(rv == 0) {rcvd.b.pb = NULL; return ERRDONE;}
		if(rv < 0 && errno == EAGAIN) return 0;
		rcvd.b.pb = NULL;
		return ERRDONE;
	}
	return INDONE;
}
//-----------------------------------------------------------------------------------------
int Station::doSend(){
	while(sndq.size()){
		Data &sndd = sndq.front();
		ssize_t rv = sd.send(sndd.b.pcb, sndd.bl, sndd.sap);
		if(rv == (ssize_t)sndd.bl){
			if(sndd.flags & RAISE_ON_DONE) rv = OUTDONE;
			sndq.pop();
			continue;
		}
		if(rv == 0) return ERRDONE;
		if(rv < 0 && errno == EAGAIN) return rv;
		cassert(!(rv > 0 && rv < (ssize_t)sndd.bl));
		return ERRDONE;
	}
	return OUTDONE;
}
//-----------------------------------------------------------------------------------------
}//namespace udp
}//namespace clientserver

