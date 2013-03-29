/* Declarations file betaclientcommand.hpp
	
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

#ifndef BETACLIENTCOMMAND_HPP
#define BETACLIENTCOMMAND_HPP


#include "algorithm/serialization/binary.hpp"
#include "system/specific.hpp"

namespace concept{

namespace beta{

namespace client{

class Command: public SpecificObject{
public:
	typedef serialization::binary::Serializer	SerializerT;
	typedef serialization::binary::Deserializer	DeserializerT;
	
	virtual ~Command();
	virtual uint32 dynamicType() const = 0;
	
	virtual void prepareSerialization(SerializerT &_rser) = 0;
	virtual void prepareDeserialization(DeserializerT &_rdes) = 0;
	virtual int executeSend(uint32 _cmdidx);
	virtual int executeRecv(uint32 _cmdidx);
protected:
	Command();
};

}//namespace client

}//namespace beta

}//namespace concept


#endif
