/* Implementation file betaconnection.cpp
	
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

#include "system/specific.hpp"
#include "betaconnection.hpp"
#include "betabuffer.hpp"

#include <string>
#include <cstring>

#include "algorithm/serialization/typemapperbase.hpp"

namespace concept{
namespace beta{


namespace {
struct TypeMapper: serialization::TypeMapperBase{
};
const TypeMapper tm;
}//namespace

/*static*/ const serialization::TypeMapperBase&	Connection::typemapper(tm);

/*static*/ const uint32							Connection::recvbufferid(Specific::sizeToIndex(4096 << 1));
/*static*/ const uint32							Connection::recvbuffercapacity(Specific::indexToCapacity(recvbufferid));

/*static*/ const uint32							Connection::sendbufferid(Specific::sizeToIndex(4096));
/*static*/ const uint32							Connection::sendbuffercapacity(Specific::indexToCapacity(sendbufferid));

/*static*/ const uint32							Connection::minsendsize(16);


Connection::Connection():
	ser(typemapper), des(typemapper), readstate(0), exception(0),
	recvbufbeg(NULL), recvbufrd(NULL), recvbufwr(NULL), recvbufend(NULL),
	sendbufbeg(NULL), sendbufend(NULL), crtcmdrecvidx(0), crtcmdsendidx(0){
}

Connection::Connection(const SocketDevice &_rsd):BaseT(_rsd),
	ser(typemapper), des(typemapper), readstate(0), exception(0),
	recvbufbeg(NULL), recvbufrd(NULL), recvbufwr(NULL), recvbufend(NULL),
	sendbufbeg(NULL), sendbufend(NULL), crtcmdrecvidx(0), crtcmdsendidx(0){
}

Connection::~Connection(){
	if(sendbufbeg){
		Specific::pushBuffer(sendbufbeg, sendbufferid);
	}
	if(recvbufbeg){
		Specific::pushBuffer(recvbufbeg, recvbufferid);
	}
}

void Connection::doPrepareRun(){
	readstate = ParseBufferHeader;
	recvbufbeg = Specific::popBuffer(recvbufferid);
	recvbufwr = recvbufrd = recvbufbeg;
	recvbufend = recvbufbeg + recvbuffercapacity;
}

void Connection::doReleaseSendBuffer(){
	if(sendbufbeg){
		Specific::pushBuffer(sendbufbeg, sendbufferid);
		sendbufbeg = NULL;
		sendbufend = NULL;
	}
}

int Connection::doFillSendBuffer(bool _usecompression, bool _useencryption){
	if(sendbufbeg == NULL){
		sendbufbeg =  Specific::popBuffer(sendbufferid);
		sendbufend = sendbufbeg + sendbuffercapacity;
	}
	char	*sendbufpos = sendbufbeg + BufferHeader::Size;
	int		writesize = doFillSendBufferData(sendbufpos);
	
	
	if(writesize <= 0){
		return writesize;
	}
	
	if(_usecompression){
		char *tmpbuf = Specific::popBuffer(sendbufferid);
		int rv = doCompressBuffer(tmpbuf + BufferHeader::Size, sendbufbeg + BufferHeader::Size, writesize);
		if(rv < 0){
			exception = ExceptionCompress;
			return BAD;
		}
		writesize = rv;
		Specific::pushBuffer(sendbufbeg, sendbufferid);
		sendbufbeg = tmpbuf;
		sendbufend = tmpbuf + sendbuffercapacity;
	}
	
	if(_useencryption){
		char *tmpbuf = Specific::popBuffer(sendbufferid);
		int rv = doEncryptBuffer(tmpbuf + BufferHeader::Size, sendbufbeg + BufferHeader::Size, writesize);
		if(rv < 0){
			exception = ExceptionEncrypt;
			return BAD;
		}
		writesize = rv;
		Specific::pushBuffer(sendbufbeg, sendbufferid);
		sendbufbeg = tmpbuf;
		sendbufend = tmpbuf + sendbuffercapacity;
	}
	
	
	BufferHeader	bufhead(
		static_cast<uint16>(writesize),
		BufferHeader::Data,
		(_useencryption ? BufferHeader::Encrypted : 0) |
		(_usecompression ? BufferHeader::Compressed : 0)
	);

	SerializerT::storeValue(sendbufbeg, bufhead.value);
	
	return writesize + BufferHeader::Size;
}



inline void Connection::doOptimizeReadBuffer(){
	if((recvbufend - recvbufrd) >= sendbuffercapacity){
		return;//we have enough space
	}else if((recvbufwr - recvbufrd) > (recvbufrd - recvbufbeg)){
		return;//we don't want to do memmove
	}
	const size_t len = recvbufwr - recvbufrd;
	memcpy(recvbufbeg, recvbufrd, len);
	recvbufrd = recvbufbeg;
	recvbufwr = recvbufbeg + len;
}

int Connection::doParseBuffer(ulong _size){
	recvbufwr += _size;
	do{
		const ulong		datasize = recvbufwr - recvbufrd;
		BufferHeader	bufhead;
		if(readstate == ParseBufferHeader){
			if(datasize < BufferHeader::Size){
				doOptimizeReadBuffer();
				return NOK;
			}
			readstate = ParseBufferData;
		}
		DeserializerT::parseValue(recvbufrd, bufhead.value);
		if(bufhead.size() > (datasize - BufferHeader::Size)){
			doOptimizeReadBuffer();
			return NOK;
		}
		if(bufhead.isEncrypted()){
			char tmpbuf[sendbuffercapacity];
			
			int sz = doDecryptBuffer(
				tmpbuf,
				recvbufrd + BufferHeader::Size,
				bufhead.size()
			);
			
			if(sz < 0){
				exception = ExceptionDecrypt;
				return BAD;
			}
			if(bufhead.isCompressed()){
				char tmpbuf2[sendbuffercapacity];
				int sz2 = doDecompressBuffer(tmpbuf2, tmpbuf, sz);
				
				if(sz < 0){
					exception = ExceptionDecompress;
					return BAD;
				}
				
				if(!doParseBufferData(tmpbuf2, sz2)){
					exception = ExceptionParse;
					return BAD;
				}
			}else if(!doParseBufferData(tmpbuf, sz)){
				return BAD;
			}
		}else if(bufhead.isCompressed()){
			char tmpbuf[sendbuffercapacity];
			
			int sz = doDecompressBuffer(
				tmpbuf,
				recvbufrd + BufferHeader::Size,
				bufhead.size()
			);
			
			if(sz < 0){
				exception = ExceptionDecompress;
				return BAD;
			}
			
			if(!doParseBufferData(tmpbuf, sz)){
				exception = ExceptionParse;
				return BAD;
			}
		}else if(bufhead.isData()){
			
			if(
				!doParseBufferData(
					recvbufrd + BufferHeader::Size,
					bufhead.size()
				)
			){
				exception = ExceptionParse;
				return BAD;
			}
		}else if(bufhead.isException()){
			return doParseBufferException(
				recvbufrd + BufferHeader::Size,
				bufhead.size()
			);
		}else{
			exception = ExceptionParseBuffer;
			return BAD;
		}
		recvbufrd += BufferHeader::Size + bufhead.size();
		readstate = ParseBufferHeader;
	}while(recvbufrd < recvbufwr);
	
	recvbufrd = recvbufwr = recvbufbeg;
	
	return OK;
}

int Connection::doParseBufferException(const char *_pbuf, ulong _len){
	exception = ExceptionParseBuffer;
	return BAD;
}

int Connection::doDecompressBuffer(char *_to, const char *_from, ulong _fromlen){
	return BAD;
}

int Connection::doDecryptBuffer(char *_to, const char *_from, ulong _fromlen){
	return BAD;
}

int Connection::doCompressBuffer(char *_to, const char *_from, ulong _fromlen){
	return BAD;
}

int Connection::doEncryptBuffer(char *_to, const char *_from, ulong _fromlen){
	return BAD;
}

void Connection::dynamicExecute(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}

int Connection::doFillSendException(){
	if(sendbufbeg == NULL){
		sendbufbeg =  Specific::popBuffer(sendbufferid);
		sendbufend = sendbufbeg + sendbuffercapacity;
	}
	char	*sendbufpos = sendbufbeg + BufferHeader::Size;
	uint32	tmp_val = exception;
	uint32	ser_err = ser.error();
	uint32	des_err = des.error();
	char	*pos = sendbufpos;
	pos = SerializerT::storeValue(pos, tmp_val);
	pos = SerializerT::storeValue(pos, crtcmdrecvidx);
	pos = SerializerT::storeValue(pos, crtcmdsendidx);
	pos = SerializerT::storeValue(pos, ser_err);
	pos = SerializerT::storeValue(pos, des_err);
	int		writesize(pos - sendbufpos);
	BufferHeader	bufhead(static_cast<uint16>(writesize),	BufferHeader::Exception, 0);

	SerializerT::storeValue(sendbufbeg, bufhead.value);
	
	return writesize + BufferHeader::Size;
}
bool Connection::doParseBufferException(
	const char *_pbuf, ulong _len,
	uint32 _error,
	uint32 _recvcmdidx,
	uint32 _sendcmdidx,
	uint32 _ser_err,
	uint32 _des_err
){
	if(_len != 5 * sizeof(uint32)){
		return false;
	}
	
	_pbuf = DeserializerT::parseValue(_pbuf, _error);
	_pbuf = DeserializerT::parseValue(_pbuf, _recvcmdidx);
	_pbuf = DeserializerT::parseValue(_pbuf, _sendcmdidx);
	_pbuf = DeserializerT::parseValue(_pbuf, _ser_err);
	_pbuf = DeserializerT::parseValue(_pbuf, _des_err);
	
	return true;
}

}//namespace beta
}//namespace concept

