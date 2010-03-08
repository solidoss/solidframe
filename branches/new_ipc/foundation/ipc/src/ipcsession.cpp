/* Implementation file ipcsession.cpp
	
	Copyright 2010 Valentin Palade
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
#include <queue>
#include <algorithm>
#include <iostream>
#include <cstring>

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "utility/queue.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "ipcsession.hpp"
#include "iodata.hpp"

namespace fdt = foundation;



namespace foundation{
namespace ipc{
//---------------------------------------------------------------------
/*static*/ void Session::init(){
}
//---------------------------------------------------------------------
/*static*/ int Session::parseAcceptedBuffer(const Buffer &_rbuf){
	return BAD;
}
//---------------------------------------------------------------------
/*static*/ int Session::parseConnectingBuffer(const Buffer &_rbuf){
	return BAD;
}
//---------------------------------------------------------------------
Session::Session(
	const Inet4SockAddrPair &_raddr,
	uint32 _keepalivetout
){
}
//---------------------------------------------------------------------
Session::Session(
	const Inet4SockAddrPair &_raddr,
	int _basport,
	uint32 _keepalivetout
){
}
//---------------------------------------------------------------------
Session::~Session(){
}
//---------------------------------------------------------------------
const Inet4SockAddrPair* Session::peerAddr4()const{
	return NULL;
}
//---------------------------------------------------------------------
const Session::Addr4PairT* Session::baseAddr4()const{
	return NULL;
}
//---------------------------------------------------------------------
const SockAddrPair* Session::peerSockAddr()const{
	return NULL;
}
//---------------------------------------------------------------------
bool Session::isConnected()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isDisconnecting()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isConnecting()const{
	return false;
}
//---------------------------------------------------------------------
bool Session::isAccepting()const{
	return false;
}
//---------------------------------------------------------------------
void Session::prepare(){
}
//---------------------------------------------------------------------
void Session::reconnect(Session *_pses){
}
//---------------------------------------------------------------------
int Session::pushSignal(DynamicPointer<Signal> &_rsig, uint32 _flag){
	return BAD;
}
//---------------------------------------------------------------------
bool Session::pushReceivedBuffer(Buffer &_rbuf, const TimeSpec &_rts){
	return false;
}
//---------------------------------------------------------------------
void Session::completeConnect(int _port){
}
//---------------------------------------------------------------------
bool Session::executeTimeout(
	Talker::TalkerStub &_rstub,
	uint32 _id,
	const TimeSpec &_rts
){
	return false;
}
//---------------------------------------------------------------------
int Session::execute(Talker::TalkerStub &_rstub, const TimeSpec &_rts){
	return BAD;
}
//---------------------------------------------------------------------
bool Session::pushSentBuffer(
	uint32 _id,
	const char *_data,
	const uint16 _size
){
	return false;
}
//---------------------------------------------------------------------

}//namespace ipc
}//namespace foundation
