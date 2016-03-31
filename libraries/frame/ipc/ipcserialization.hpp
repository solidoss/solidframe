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

struct ConnectionContext;

using MessageCompleteFunctionT	= FUNCTION<void(
	ConnectionContext &, MessagePointerT &, MessagePointerT &, ErrorConditionT const &
)>;

struct TypeStub{
	MessageCompleteFunctionT	complete_fnc;
};

using SerializerT				= serialization::binary::Serializer<ConnectionContext>;
using DeserializerT 			= serialization::binary::Deserializer<ConnectionContext>;
using TypeIdMapT				= serialization::TypeIdMap<SerializerT, DeserializerT, TypeStub>;

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
