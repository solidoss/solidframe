/* Declarations file service.hpp
	
	Copyright 2013 Valentin Palade 
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

#ifndef SOLID_FRAME_SERVICE_HPP
#define SOLID_FRAME_SERVICE_HPP

#include "frame/common.hpp"
#include "system/mutex.hpp"
#include "utility/dynamictype.hpp"
#include <vector>

namespace solid{
namespace frame{

class	Manager;
struct	Message;
class	Object;

typedef DynamicSharedPointer<Message>	MessageSharedPointerT;
typedef DynamicPointer<Message>			MessagePointerT;

class Service: public Dynamic<Service>{
public:
	Service(
		Manager &_rm
	);
	~Service();
	
	ObjectUidT registerObject(Object &_robj);
	
	bool notifyAll(ulong _sm);

	bool notifyAll(MessagePointerT &_rmsgptr);
	
	void reset();
	
	void stop(bool _wait = true);
};

}//namespace frame
}//namespace solid

#endif
