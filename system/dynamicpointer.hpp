/* Declarations file dynamicpointer.hpp
	
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

#ifndef SYSTEM_DYNAMIC_POINTER_HPP
#define SYSTEM_DYNAMIC_POINTER_HPP

struct DynamicBase;

class DynamicPointerBase{
protected:
	void clear(DynamicBase *_pdyn);
	void use(DynamicBase *_pdyn);

};
//! An autoptr like smartpointer for signals
template <class T>
class DynamicPointer: DynamicPointerBase{
public:
	typedef DynamicPointer<T>	DynamicPointerTp;
	typedef T					DynamicTp;
	
	explicit DynamicPointer(DynamicTp *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn) use(static_cast<DynamicBase*>(_pdyn));
	}
	DynamicPointer(const DynamicPointerTp &_rcp):pdyn(_rcp.release()){}
	~DynamicPointer(){
		if(pdyn){DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));}
	}
	DynamicTp* release() const{
		DynamicTp *tmp = pdyn;
		pdyn = NULL;return tmp;
	}
	DynamicPointerTp& operator=(const DynamicPointerTp &_rcp){
		if(pdyn) clear();
		pdyn = _rcp.release();
		return *this;
	}
	DynamicPointerTp& operator=(DynamicTp *_pdyn){
		clear();ptr(_pdyn);	return *this;
	}
	DynamicTp& operator*()const {return *pdyn;}
	DynamicTp* operator->()const{return pdyn;}
	DynamicTp* ptr() const		{return pdyn;}
	//operator bool () const	{return psig;}
	bool operator!()const	{return !pdyn;}
	operator bool()	const	{return pdyn != NULL;}
	void clear(){if(pdyn) DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));pdyn = NULL;}
protected:
	void ptr(DynamicTp *_pdyn){
		pdyn = _pdyn;use(static_cast<DynamicBase*>(pdyn));
	}
private:
	mutable DynamicTp *pdyn;
};


#endif
