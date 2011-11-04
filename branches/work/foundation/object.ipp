/* Inline implementation file object.ipp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef NINLINES
#define inline
#else
#include "foundation/manager.hpp"
namespace foundation{
#endif



inline void Object::getThread(uint32 &_rthid, uint32 &_rthpos)const{
	//which is better:
	//new thread id and old pos, or
	//new pos and old thread id
	_rthpos = thrpos;
	_rthid  = thrid;
}
inline void Object::setThread(uint32 _thrid, uint32 _thrpos){
	thrid  = _thrid;
	thrpos = _thrpos;
}

inline ulong Object::grabSignalMask(ulong _leave){
	ulong sm = smask;
	smask = sm & _leave;
	return sm;
}
inline bool Object::signaled(ulong _s) const{
	return (smask & _s) != 0;
}
inline void Object::id(IndexT _fullid){
	fullid = _fullid;
}
inline void Object::id(IndexT _srvidx, IndexT _objidx){
	fullid = Manager::the().computeId(_srvidx, _objidx);
}

inline void Object::state(int _st){
	crtstate = _st;//if state < 0 the object can be destroyed
}
inline IndexT Object::serviceId()const{
	return Manager::the().computeServiceId(fullid);
}
inline IndexT Object::index()const{
	return Manager::the().computeIndex(fullid);
}

#ifdef NINLINES
#undef inline
#else
}//namespace
#endif
