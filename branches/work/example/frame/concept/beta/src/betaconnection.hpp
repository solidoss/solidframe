// betaconnection.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETACONNECTION_HPP
#define BETACONNECTION_HPP


#include "system/common.hpp"
#include "system/socketdevice.hpp"

#include "utility/dynamictype.hpp"
#include "utility/stack.hpp"
#include "utility/queue.hpp"

#include "serialization/binary.hpp"

#include "frame/aio/aiosingleobject.hpp"

#include <openssl/ossl_typ.h>

namespace solid{
namespace serialization{
class TypeMapperBase;
}
}
using solid::uint32;
using solid::uint8;


namespace concept{

namespace beta{

class Service;

class Connection: public solid::Dynamic<Connection, solid::frame::aio::SingleObject>{
public:
	typedef Service	ServiceT;
	
protected:
	typedef solid::serialization::binary::Serializer<void>		SerializerT;
	typedef solid::serialization::binary::Deserializer<void>	DeserializerT;
	
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
	
	static const solid::serialization::TypeMapperBase	&typemapper;
	static const uint32									recvbufferid;
	static const uint32									recvbuffercapacity;

	static const uint32									sendbufferid;
	static const uint32									sendbuffercapacity;

	static const uint32									minsendsize;
protected:
	Connection(const solid::SocketDevice &_rsd);
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
