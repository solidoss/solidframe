/* Declarations file selectorbase.hpp
	
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

#ifndef SOLID_FRAME_SELECTOR_BASE_HPP
#define SOLID_FRAME_SELECTOR_BASE_HPP

#include "frame/object.hpp"

namespace solid{
namespace frame{

class Manager;

//! The base for every selector
/*!
 * The manager will call raise when an object needs processor
 * time, e.g. because of an event.
 */
class SelectorBase{
public:
	uint32 id()const;
	virtual void raise(uint32 _objidx = 0) = 0;
protected:
	void associateObjectToCurrentThread(Object &_robj);
	void setObjectThread(Object &_robj, const IndexT &_objidx);
	int executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout);
	void setCurrentTimeSpecific(const TimeSpec &_rtout);
	void id(uint32 _id);
private:
	friend class Manager;
	uint32	selid;//given by manager
};

inline uint32 SelectorBase::id()const{
	return selid;
}

inline void SelectorBase::associateObjectToCurrentThread(Object &_robj){
	_robj.associateToCurrentThread();
}
inline int SelectorBase::executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout){
	return _robj.execute(_evs, _rtout);
}

inline void SelectorBase::id(uint32 _id){
	selid = _id;
}
inline void SelectorBase::setCurrentTimeSpecific(const TimeSpec &_rtout){
	Object::doSetCurrentTime(&_rtout);
}

}//namespace frame
}//namespace solid

#endif

