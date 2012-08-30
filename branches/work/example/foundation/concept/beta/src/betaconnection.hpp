/* Declarations file betaconnection.hpp
	
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

#ifndef BETACONNECTION_HPP
#define BETACONNECTION_HPP


#include "system/common.hpp"
#include "system/socketdevice.hpp"

#include "utility/dynamictype.hpp"
#include "utility/stack.hpp"
#include "utility/queue.hpp"

#include "algorithm/serialization/binary.hpp"

#include "foundation/aio/aiosingleobject.hpp"

#include <openssl/ossl_typ.h>


namespace serialization{
class TypeMapperBase;
}


namespace concept{

namespace beta{

class Service;

class Connection: public Dynamic<Connection, foundation::aio::SingleObject>{
public:
	typedef Service	ServiceT;
	
	void dynamicExecute(DynamicPointer<> &_dp);
protected:
	typedef DynamicExecuter<
		void,
		Connection,
		foundation::DynamicServicePointerStore,
		void
	>	DynamicExecuterT;
	typedef serialization::binary::Serializer	SerializerT;
	typedef serialization::binary::Deserializer	DeserializerT;
	
	enum{
		ParseBufferHeader = 1,
		ParseBufferData
	};
	enum{
		ExceptionCompress = 1,
		ExceptionDecompress,
		ExceptionEncrypt,
		ExceptionDecrypt,
		ExceptionParse,
		ExceptionFill,
		ExceptionParseBuffer,
		
	};
	
	static const serialization::TypeMapperBase	&typemapper;
	static const uint32							recvbufferid;
	static const uint32							recvbuffercapacity;

	static const uint32							sendbufferid;
	static const uint32							sendbuffercapacity;

	static const uint32							minsendsize;
protected:
	Connection(const SocketDevice &_rsd);
	Connection();
	~Connection();
	void doPrepareRun();
	int doParseBuffer(ulong _size);
	int doDecompressBuffer(char *_to, const char *_from, ulong _fromlen);
	int doDecryptBuffer(char *_to, const char *_from, ulong _fromlen);
	int doCompressBuffer(char *_to, const char *_from, ulong _fromlen);
	int doEncryptBuffer(char *_to, const char *_from, ulong _fromlen);
	int doFillSendBuffer(bool _usecompression, bool _useencryption);
	virtual bool doParseBufferData(const char *_pbuf, ulong _len) = 0;
	virtual int	doFillSendBufferData(char *_sendbufpos) = 0;
	virtual int doParseBufferException(const char *_pbuf, ulong _len);
	void doReleaseSendBuffer();
	int doFillSendException();
	bool doParseBufferException(
		const char *_pbuf, ulong _len,
		uint32 _error,
		uint32 _recvcmdidx,
		uint32 _sendcmdidx,
		uint32 _ser_err,
		uint32 _des_err
	);
private:
	void doOptimizeReadBuffer();
protected:
	SerializerT					ser;
	DeserializerT				des;
	DynamicExecuterT			de;
	uint8						readstate;
	uint8						exception;
	char						*recvbufbeg;
	char						*recvbufrd;
	char						*recvbufwr;
	char						*recvbufend;
	char						*sendbufbeg;
	char						*sendbufend;
	uint32						crtcmdrecvidx;
	uint32						crtcmdsendidx;
};



}//namespace beta

}//namespace concept

#endif
