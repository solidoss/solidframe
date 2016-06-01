// frame/ipc/ipcprotocol_binary_serialization_v1.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_IPC_IPCPROTOCOL_SERIALIZATION_V1_HPP
#define SOLID_FRAME_IPC_IPCPROTOCOL_SERIALIZATION_V1_HPP

#include "frame/ipc/ipcprotocol.hpp"
#include "serialization/binary.hpp"
#include "serialization/binarybasic.hpp"
#include "serialization/typeidmap.hpp"
#include "system/specific.hpp"

namespace solid{
namespace frame{
namespace ipc{
namespace serialization_v1{

using SerializerT				= serialization::binary::Serializer<ConnectionContext>;
using DeserializerT 			= serialization::binary::Deserializer<ConnectionContext>;
using TypeIdMapT				= serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>;
using LimitsT					= serialization::binary::Limits;
	
struct Serializer: public ipc::Serializer, public SpecificObject{
	SerializerT		ser;
	
	Serializer(const LimitsT &_rlimits, TypeIdMapT const &_ridmap): ser(_rlimits, &_ridmap){}
	
	/*virtual*/ void push(MessagePointerT &_rmsgptr, const size_t _msg_type_idx) override{
		ser.push(_rmsgptr, _msg_type_idx, "message");
	}
	/*virtual*/ int run(ConnectionContext &_rctx, char* _pdata, size_t _data_len) override{
		return ser.run(_pdata, _data_len, _rctx);
	}
	/*virtual*/ ErrorConditionT error() const override{
		return ser.error();
	}
	
	/*virtual*/ bool empty() const override{
		return ser.empty();
	}
	
	/*virtual*/ void clear() override{
		return ser.clear();
	}
};


struct Deserializer: public ipc::Deserializer, public SpecificObject{
	DeserializerT	des;
	
	Deserializer(const LimitsT &_rlimits, TypeIdMapT const &_ridmap): des(_rlimits, &_ridmap){}
	
	/*virtual*/ void push(MessagePointerT &_rmsgptr) override{
		des.push(_rmsgptr, "message");
	}
	/*virtual*/ int run(ConnectionContext &_rctx, const char* _pdata, size_t _data_len) override{
		return des.run(_pdata, _data_len, _rctx);
	}
	/*virtual*/ ErrorConditionT error() const override{
		return des.error();
	}
	
	/*virtual*/ bool empty() const override{
		return des.empty();
	}
	/*virtual*/ void clear() override{
		return des.clear();
	}
};


struct Protocol: public ipc::Protocol{
	TypeIdMapT		type_map;
	
	LimitsT			limits;
	
	Protocol(){}
	Protocol(const LimitsT &_limits):limits(_limits){}
	
	ProtocolPointerT pointerFromThis(){
		return ProtocolPointerT(this, [](ipc::Protocol*){});
	}
	
	template <class T, class FactoryFnc>
	size_t registerType(
		FactoryFnc _facf,
		const size_t _protocol_id,
		const size_t _idx = 0
	){
		TypeStub ts;
		size_t rv = type_map.registerType<T>(_facf, ts, _protocol_id, _idx);
		registerCast<T, ipc::Message>();
		return rv;
	}
	
	template <class Msg, class FactoryFnc, class CompleteFnc>
	size_t registerType(
		FactoryFnc _factory_fnc,
		CompleteFnc _complete_fnc,
		const size_t _protocol_id = 0,
		const size_t _idx = 0
	){
		TypeStub ts;
		
		using CompleteHandlerT = CompleteHandler<
			CompleteFnc,
			typename message_complete_traits<decltype(_complete_fnc)>::send_type,
			typename message_complete_traits<decltype(_complete_fnc)>::recv_type
		>;
		
		ts.complete_fnc = MessageCompleteFunctionT(CompleteHandlerT(_complete_fnc));
		
		size_t rv = type_map.registerType<Msg>(
			ts, Message::serialize<SerializerT, Msg>, Message::serialize<DeserializerT, Msg>, _factory_fnc,
			_protocol_id, _idx
		);
		registerCast<Msg, ipc::Message>();
		return rv;
	}
	
	template <class Derived, class Base>
	bool registerCast(){
		return type_map.registerCast<Derived, Base>();
	}
	
	template <class Derived, class Base>
	bool registerDownCast(){
		return type_map.registerDownCast<Derived, Base>();
	}
	
	
	/*virtual*/ char* storeValue(char *_pd, uint8  _v) const override{
		return serialization::binary::store(_pd, _v);
	}
	/*virtual*/ char* storeValue(char *_pd, uint16 _v) const override{
		return serialization::binary::store(_pd, _v);
	}
	/*virtual*/ char* storeValue(char *_pd, uint32 _v) const override{
		return serialization::binary::store(_pd, _v);
	}
	/*virtual*/ char* storeValue(char *_pd, uint64 _v) const override{
		return serialization::binary::store(_pd, _v);
	}
	
	/*virtual*/ const char* loadValue(const char *_ps, uint8  &_v) const override{
		return serialization::binary::load(_ps, _v);
	}
	/*virtual*/ const char* loadValue(const char *_ps, uint16 &_v) const override{
		return serialization::binary::load(_ps, _v);
	}
	/*virtual*/ const char* loadValue(const char *_ps, uint32 &_v) const override{
		return serialization::binary::load(_ps, _v);
	}
	/*virtual*/ const char* loadValue(const char *_ps, uint64 &_v) const override{
		return serialization::binary::load(_ps, _v);
	}
	
	/*virtual*/ size_t typeIndex(const Message *_pmsg) const override{
		return type_map.index(_pmsg);
	}
	
	/*virtual*/ const TypeStub& operator[](const size_t _idx) const override{
		return type_map[_idx];
	}
	
	/*virtual*/ SerializerPointerT   createSerializer() const override{
		return SerializerPointerT(new Serializer(limits, type_map));
	}
	/*virtual*/ DeserializerPointerT createDeserializer() const override{
		return DeserializerPointerT(new Deserializer(limits, type_map));
	}
	
	/*virtual*/ void reset(ipc::Serializer &_ser) const override{
		static_cast<Serializer&>(_ser).ser.resetLimits();
	}
	/*virtual*/ void reset(ipc::Deserializer &_des) const override{
		static_cast<Deserializer&>(_des).des.resetLimits();
	}
	
	/*virtual*/ size_t minimumFreePacketDataSize() const override{
		return 16;
	}
};



}//namespace serialization_v1
}//namespace ipc
}//namespace frame
}//namespace solid

#endif
