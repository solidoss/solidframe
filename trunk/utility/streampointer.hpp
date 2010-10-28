/* Declarations file streampointer.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef UTILITY_STREAM_POINTER_HPP
#define UTILITY_STREAM_POINTER_HPP


#include "system/common.hpp"

class Stream;

struct StreamPointerBase{
protected:
	void clear(Stream *_pobj);
};

//! An autoptr type of smart pointer for streams
/*!
	When the strea must be deleted (e.g. on clear), Stream::release is called and if it 
	returns true then it will be acctually deleted.
*/
template <class SO>
class StreamPointer: StreamPointerBase{
public:
	typedef SO 				ObjectT;
	typedef StreamPointer<SO>	ThisT;
	StreamPointer():pobj(NULL){}
	StreamPointer(ObjectT *_pobj):pobj(_pobj) {}
	StreamPointer(const ThisT &_pobj):pobj(_pobj.release()){}
	~StreamPointer(){
		if(pobj){StreamPointerBase::clear(pobj);}
	}
	ObjectT* release() const{
		ObjectT *po = pobj;
		pobj = NULL;//TODO:
		return po;
	}
	ThisT& operator=(const ThisT &_robj){
		if(pobj) clear();
		pobj = _robj.release();
		return *this;
	}
	ThisT& operator=(ObjectT *_pobj){
		if(pobj) clear();
		pobj = _pobj;
		return *this;
	}
	inline ObjectT& operator*()const throw() {return *pobj;}
	inline ObjectT* operator->()const throw() {return pobj;}
	ObjectT* ptr() const throw() {return pobj;}
	operator bool () const throw() {return pobj;}
	bool operator!()const throw() {return !pobj;}
	void clear(){if(pobj){StreamPointerBase::clear(pobj);pobj = NULL;}}
private:
	mutable ObjectT 	*pobj;
};

#endif
