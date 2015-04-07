// frame/ipc/ipcservice.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCSERVICE_HPP
#define SOLID_FRAME_IPC_IPCSERVICE_HPP

#include "serialization/binary.hpp"

#include "frame/service.hpp"
#include "frame/aio/aioreactor.hpp"
#include "frame/scheduler.hpp"
#include "frame/ipc/ipcsessionuid.hpp"
#include "frame/ipc/ipcmessage.hpp"

namespace solid{

struct SocketAddressStub;
struct SocketDevice;
struct ResolveIterator;


namespace frame{

namespace aio{
class Object;

namespace openssl{
class Context;
}

}

namespace ipc{

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;

template <class T>
MessagePointerT factory(){
	return MessagePointerT(new T);
}

struct Configuration{
	template <class F>
	ErrorConditionT protocolCallback(F _f){
		
		return ErrorConditionT();
	}
};


struct MessageRegisterProxy{
	template <class Msg, class FactoryFnc, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	void registerMessage(FactoryFnc _facf, ReceiveFnc _regf, PrepareFnc _prepf, CompleteFnc _cmpltf, size_t _idx = -1){
		
	}
	
private:
	friend class Service;
	MessageRegisterProxy(Service &_rs):rs(_rs){}
	Service &rs;
};

//! Inter Process Communication service
/*!
	Allows exchanging ipc::Messages between processes.
	<br>
*/
class Service: public Dynamic<Service, frame::Service>{
public:
	typedef serialization::binary::Serializer<
		const ConnectionContext
	>						SerializerT;
	typedef serialization::binary::Deserializer<
		const ConnectionContext
	>						DeserializerT;
private:
	
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	static const char* errorText(int _err);
	
	Service(
		frame::Manager &_rm, frame::Event const &_revt
	);
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration const& _rcfg);
	
	
private:
private:
	struct Data;
	Data		&d;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
