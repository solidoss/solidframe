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

#include "serialization/idtypemapper.hpp"
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


struct Configuration{
	template <class F>
	void registerMessages(F _f){
		
	}
};


struct RegisterProxy{
	template <class Msg, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	void registerMessage(ReceiveFnc _regf, PrepareFnc _prepf, CompleteFnc _cmpltf){
		
	}
	
private:
	friend class Service;
	RegisterProxy(Service &_rs):rs(_rs){}
	Service &rs;
};

//! An Inter Process Communication service
/*!
	Allow for sending/receiving serializable ipc::Messages between processes.
	A process is identified by a pair [IP address, port].<br>
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
	typedef serialization::IdTypeMapper<
		SerializerT,
		DeserializerT,
		SerializationTypeIdT
	>						IdTypeMapperT;
	
	struct Handle{
		bool checkStore(void */*_pt*/, const ConnectionContext &/*_rctx*/)const{
			return true;
		}
		bool checkLoad(void */*_pt*/, const ConnectionContext &/*_rctx*/)const{
			return true;
		}
		void afterSerialization(SerializerT &_rs, void *_pm, const ConnectionContext &_rctx){}
		void afterSerialization(DeserializerT &_rs, void *_pm, const ConnectionContext &_rctx){}
	
		void beforeSerialization(SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx);
		void beforeSerialization(DeserializerT &_rs, Message *_pt, const ConnectionContext &_rctx);
	};
	
	template <class H>
	struct ProxyHandle: H, Handle{
		void beforeSerialization(SerializerT &_rs, Message *_pt, const ConnectionContext &_rctx){
			this->Handle::beforeSerialization(_rs, _pt, _rctx);
		}
		void beforeSerialization(DeserializerT &_rd, Message *_pt, const ConnectionContext &_rctx){
			this->Handle::beforeSerialization(_rd, _pt, _rctx);
		}
	};
	
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	static const char* errorText(int _err);
	
	Service(
		frame::Manager &_rm, frame::Event const &_revt
	);
	
	template <class T>
	uint32 registerMessageType(uint32 _idx = 0){
		return typeMapper().insertHandle<T, Handle>(_idx);
	}
	
	template <class T, class H>
	uint32 registerMessageType(uint32 _idx = 0){
		return typeMapper().insertHandle<T, ProxyHandle<Handle> >(_idx);
	}
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration const& _rcfg);
	
	
private:
	const serialization::TypeMapperBase& typeMapperBase() const;

	IdTypeMapperT& typeMapper();
private:
	struct Data;
	Data			&d;
	IdTypeMapperT	typemapper;
};



inline const serialization::TypeMapperBase& Service::typeMapperBase() const{
	return typemapper;
}

inline Service::IdTypeMapperT& Service::typeMapper(){
	return typemapper;
}



}//namespace ipc
}//namespace frame
}//namespace solid

#endif
