// frame/ipc/ipcserialization.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCSERIALIZATION_HPP
#define SOLID_FRAME_IPC_IPCSERIALIZATION_HPP


#include "ipcmessage.hpp"
#include "serialization/binary.hpp"
#include "system/function.hpp"


namespace solid{
namespace frame{
namespace ipc{


enum MessageFlags{
	MessageRequestFlagE		= 1,
	MessageResponseFlagE	= 2,
	MessageSynchronousFlagE = 4,
};


struct ConnectionContext;

typedef FUNCTION<void(ConnectionContext &, MessagePointerT &)>							MessageReceiveFunctionT;
typedef FUNCTION<void(ConnectionContext &, MessagePointerT &, ErrorConditionT const &)>	MessageCompleteFunctionT;
typedef FUNCTION<void(ConnectionContext &, Message const &)>							MessagePrepareFunctionT;

struct TypeStub{
	MessagePrepareFunctionT		prepare_fnc;
	MessageReceiveFunctionT		receive_fnc;
	MessageCompleteFunctionT	complete_fnc;
};

typedef serialization::binary::Serializer<ConnectionContext>							SerializerT;
typedef serialization::binary::Deserializer<ConnectionContext>							DeserializerT;
typedef serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>					TypeIdMapT;

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
