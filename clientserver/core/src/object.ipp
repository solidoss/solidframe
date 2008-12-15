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

#ifndef UINLINES
#define inline
#endif



enum ObjectDefs{
	SERVICEBITCNT = sizeof(ulong) == 4 ? 5 : 8,
	INDEXBITCNT	= sizeof(ulong) * 8 - SERVICEBITCNT,
};

inline void Object::getThread(uint &_rthid, uint &_rthpos){
	//which is better:
	//new thread id and old pos, or
	//new pos and old thread id
	_rthpos = thrpos;
	_rthid  = thrid;
}
inline void Object::setThread(uint _thrid, uint _thrpos){
	thrid  = _thrid;
	thrpos = _thrpos;
}
inline ulong Object::computeIndex(ulong _fullid){
	return _fullid & (MAX_ULONG >> SERVICEBITCNT);
}
inline ulong Object::computeServiceId(ulong _fullid){
	return _fullid >> INDEXBITCNT;
}

inline ulong Object::grabSignalMask(ulong _leave){
	ulong sm = smask;
	smask = sm & _leave;
	return sm;
}
inline uint Object::signaled(uint _s) const{
	return smask & _s;
}
inline void Object::id(ulong _fullid){
	fullid = _fullid;
}
inline void Object::id(ulong _srvid, ulong _ind){
	fullid = (_srvid << INDEXBITCNT) | _ind;
}
inline void Object::state(int _st){
	crtstate = _st;//if state < 0 the object can be destroyed
}
inline uint Object::serviceid()const{
	return computeServiceId(fullid);
}
inline ulong Object::index()const{
	return computeIndex(fullid);
}

#ifndef UINLINES
#undef inline
#endif
