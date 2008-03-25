/* Inline implementation file station.ipp
	
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

#ifndef UINLINES
#define inline
#endif

inline Station::Data::Data(char *_pb, uint _bl, uint _flags):bl(_bl),flags(0){
	b.pb = _pb;
}
inline Station::Data::Data(
	const SockAddrPair &_sap, 
	char *_pb, 
	uint _bl, 
	uint _flags
):bl(_bl), flags(0), sap(_sap){
	b.pb = _pb;
}
inline Station::Data::Data(
	const SockAddrPair &_sap, 
	const char *_pb, 
	uint _bl, 
	uint _flags
):bl(_bl), flags(0), sap(_sap){
	b.pcb = _pb;
}

inline uint Station::recvSize()const{
	return rcvsz;
}
inline const SockAddrPair &Station::recvAddr()const{
	return rcvd.sap;
}
inline int Station::descriptor()const{
	return sd;
}
inline ulong Station::ioRequest()const{
	ulong rv = rcvd.b.pb ? INTOUT : 0;
	if(sndq.size()) rv |= OUTTOUT;
	return rv;
}
inline uint Station::sendPendingCount()const{
	return sndq.size();
}
inline bool Station::recvPending()const{
	return rcvd.b.pb;
}

#ifndef UINLINES
#undef inline
#endif
