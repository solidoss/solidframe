// betaservercommand.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETASERVERCOMMAND_HPP
#define BETASERVERCOMMAND_HPP


#include "serialization/binary.hpp"
#include "system/specific.hpp"

using solid::uint32;

namespace concept{

namespace beta{

namespace server{

class Command: public solid::SpecificObject{
public:
	typedef solid::serialization::binary::Serializer<void>		SerializerT;
	typedef solid::serialization::binary::Deserializer<void>	DeserializerT;
	
	virtual ~Command();
	
	virtual void prepareSerialization(
		SerializerT &_rser
	) = 0;
	virtual void prepareDeserialization(
		DeserializerT &_rdes
	) = 0;
	
	virtual int executeRecv(uint32 _cmdidx);
	virtual int executeSend(uint32 _cmdidx);
	
	virtual uint32 dynamicType() const = 0;
protected:
	Command();
};


}//namespace server

}//namespace beta

}//namespace concept

#endif
