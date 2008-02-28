/* Inline implementation file channel.ipp
	
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

inline const uint64& Channel::recvCount()const{return rcvcnt;}
inline const uint64& Channel::sendCount()const{return sndcnt;}
inline int Channel::ok()const{return sd >= 0;}

inline int Channel::send(const char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doSendSecure(_pb, _blen, _flags);
	else	return doSendPlain(_pb, _blen, _flags);
}
inline int Channel::recv(char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doRecvPlain(_pb, _blen, _flags);
	else	return doRecvPlain(_pb, _blen, _flags);
}

inline int Channel::send(IStreamIterator& _rsi, uint64 _slen, char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doSendSecure(_rsi, _slen, _pb, _blen, _flags);
	else	return doSendPlain(_rsi, _slen, _pb, _blen, _flags);
}

inline int Channel::recv(OStreamIterator& _rsi, uint64 _slen, char* _pb, uint32 _blen, uint32 _flags){
	if(psch)return doRecvSecure(_rsi, _slen, _pb, _blen, _flags);
	else	return doRecvPlain(_rsi, _slen, _pb, _blen, _flags);
}

inline int Channel::doRecv(){
	if(psch)return doRecvSecure();
	else	return doRecvPlain();
}

inline int Channel::doSend(){
	if(psch)return doSendSecure();
	else	return doSendPlain();
}

#ifndef UINLINES
#undef inline
#endif
