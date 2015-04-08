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

#include "system/function.hpp"
#include "system/exception.hpp"
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
MessagePointerT factory(ConnectionContext &){
	return MessagePointerT(new T);
}

struct MessageRegisterProxy;
class Service;

struct Configuration{
	typedef FUNCTION<void(MessageRegisterProxy &_rproxy)>	MessageRegisterFunctionT;
	
	
	template <class F>
	void protocolCallback(F _f);
private:
	MessageRegisterFunctionT	regfnc;
};


//! Inter Process Communication service
/*!
	Allows exchanging ipc::Messages between processes.
	<br>
*/
class Service: public Dynamic<Service, frame::Service>{
public:
	typedef Dynamic<Service, frame::Service> BaseT;
	
	Service(
		frame::Manager &_rm, frame::Event const &_revt
	);
	
	//! Destructor
	~Service();

	ErrorConditionT reconfigure(Configuration const& _rcfg);
	
	
private:
	typedef serialization::binary::Serializer<ConnectionContext	>						SerializerT;
	typedef serialization::binary::Deserializer<ConnectionContext>						DeserializerT;
	
	typedef FUNCTION<MessagePointerT(ConnectionContext &)>								MessageFactoryFunctionT;
	typedef FUNCTION<void(ConnectionContext &, MessagePointerT &)>						MessageReceiveFunctionT;
	typedef FUNCTION<void(ConnectionContext &, MessagePointerT &, ErrorCodeT const &)>	MessageCompleteFunctionT;
	typedef FUNCTION<uint32(ConnectionContext &, Message const &)>						MessagePrepareFunctionT;
	typedef FUNCTION<void(ConnectionContext &, Message&, SerializerT &)>				MessageStoreFunctionT;
	typedef FUNCTION<void(ConnectionContext &, Message&, DeserializerT &)>				MessageLoadFunctionT;
	
	template <class F, class M>
	struct ReceiveProxy{
		F	f;
		ReceiveProxy(F _f):f(_f){}
		
		void operator()(ConnectionContext &_rctx, MessagePointerT &_rmsgptr){
			DynamicPointer<M>	msgptr(std::move(_rmsgptr));
			f(_rctx, msgptr);
		}
	};
	
	template <class F, class M>
	struct CompleteProxy{
		F	f;
		CompleteProxy(F _f):f(_f){}
		
		void operator()(ConnectionContext &_rctx, MessagePointerT &_rmsgptr, ErrorCodeT const &_err){
			DynamicPointer<M>	msgptr(std::move(_rmsgptr));
			f(_rctx, msgptr, _err);
		}
	};
	
	template <class F, class M>
	struct PrepareProxy{
		F	f;
		PrepareProxy(F _f):f(_f){}
		
		uint32 operator()(ConnectionContext &_rctx, Message const &_rmsg){
			M const	&rmsg = static_cast<M const &>(_rmsg);
			return f(_rctx, rmsg);
		}
	};
	
	template <class M>
	static void store_message(ConnectionContext &_rctx, Message &_rmsg, SerializerT &_rser){
		M	&rmsg = static_cast<M&>(_rmsg);
		rmsg.serialize(_rser, _rctx);
	}
	
	template <class M>
	static void load_message(ConnectionContext &_rctx, Message &_rmsg, DeserializerT &_rdes){
		M	&rmsg = static_cast<M&>(_rmsg);
		rmsg.serialize(_rdes, _rctx);
	}
	
	friend struct MessageRegisterProxy;
	template <class Msg, class FactoryFnc, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	void registerMessage(FactoryFnc _facf, ReceiveFnc _rcvf, PrepareFnc _prepf, CompleteFnc _cmpltf, size_t _idx = -1){
		MessageFactoryFunctionT		factoryfnc(_facf);
		MessageReceiveFunctionT		receivefnc = MessageReceiveFunctionT(ReceiveProxy<ReceiveFnc, Msg>(_rcvf));
		MessagePrepareFunctionT		preparefnc = MessagePrepareFunctionT(PrepareProxy<PrepareFnc, Msg>(_prepf));
		MessageCompleteFunctionT	completefnc = MessageCompleteFunctionT(CompleteProxy<CompleteFnc, Msg>(_cmpltf));
		MessageStoreFunctionT		storefnc(store_message<Msg>);
		MessageLoadFunctionT		loadfnc(load_message<Msg>);
		
		DynamicIdVectorT 			typeidvec;
		
		Msg::staticTypeIds(typeidvec);
		
		doRegisterMessage(
			typeidvec,
			factoryfnc,
			storefnc,
			loadfnc,
			receivefnc,
			preparefnc,
			completefnc,
			_idx
		);
	}
	void doRegisterMessage(
		DynamicIdVectorT const& 	_rtypeidvec,
		MessageFactoryFunctionT&	_rfactoryfnc,
		MessageStoreFunctionT&		_rstorefnc,
		MessageLoadFunctionT&		_rloadfnc,
		MessageReceiveFunctionT&	_rreceivefnc,
		MessagePrepareFunctionT&	_rpreparefnc,
		MessageCompleteFunctionT&	_rcompletefnc,
		size_t	_idx
	);
private:
	struct	Data;
	Data	&d;
};

struct MessageRegisterProxy{
	template <class Msg, class FactoryFnc, class ReceiveFnc, class PrepareFnc, class CompleteFnc>
	ErrorConditionT registerMessage(FactoryFnc _facf, ReceiveFnc _regf, PrepareFnc _prepf, CompleteFnc _cmpltf, size_t _idx = -1){
		ErrorConditionT	err;
		if(!isOk()){
			err.assign(-1, err.category());
			return err;
		}
		if(pservice){
			pservice->registerMessage<Msg>(_facf, _regf, _prepf, _cmpltf, _idx);
		}else{
			DynamicIdVectorT idvec;
			Msg::staticTypeIds(idvec);
			if(!check(idvec)){
				THROW_EXCEPTION("Invalid message list - Message's dynamic typeids overlap");
				err.assign(-1, err.category());
				pass_test = false;
				return err;
			}
			idvec.clear();
		}
		return err;
	}
	~MessageRegisterProxy();
	bool check(DynamicIdVectorT const &_idvec);
	bool isOk()const{
		return pass_test;
	}
private:
	friend class Service;
	friend class Configuration;
	struct Data;
	MessageRegisterProxy();
	MessageRegisterProxy(Service &_rservice):pservice(&_rservice), pd(nullptr){}
	Service *pservice;
	Data	*pd;
	bool	pass_test;
};


template <class F>
void Configuration::protocolCallback(F _f){
	MessageRegisterProxy	proxy;
	_f(proxy);//may throw
	if(proxy.isOk()){
		regfnc = _f;
	}
}

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
