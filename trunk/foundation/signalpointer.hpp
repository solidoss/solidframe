/* Declarations file signalpointer.hpp
	
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

#ifndef CLIENT_SERVER_SIGPTR_HPP
#define CLIENT_SERVER_SIGPTR_HPP

#include "common.hpp"

namespace foundation{

class Signal;

class SignalPointerBase{
protected:
	void clear(Signal *_psig);
	void use(Signal *_psig);

};
//! An autoptr like smartpointer for signals
template <class T>
class SignalPointer:SignalPointerBase{
public:
	typedef SignalPointer<T>	SignalPointerTp;
	typedef T					SignalTp;
	
	explicit SignalPointer(SignalTp *_psig = NULL):psig(_psig){
		if(_psig) use(static_cast<Signal*>(_psig));
	}
	SignalPointer(const SignalPointerTp &_rcp):psig(_rcp.release()){}
	~SignalPointer(){
		if(psig){SignalPointerBase::clear(static_cast<Signal*>(psig));}
	}
	SignalTp* release() const{
		SignalTp *tmp = psig;
		psig = NULL;return tmp;
	}
	SignalPointerTp& operator=(const SignalPointerTp &_rcp){
		if(psig) clear();
		psig = _rcp.release();
		return *this;
	}
	SignalPointerTp& operator=(SignalTp *_psig){
		clear();ptr(_psig);	return *this;
	}
	SignalTp& operator*()const {return *psig;}
	SignalTp* operator->()const{return psig;}
	SignalTp* ptr() const		{return psig;}
	//operator bool () const	{return psig;}
	bool operator!()const	{return !psig;}
	operator bool()	const	{return psig != NULL;}
	void clear(){if(psig) SignalPointerBase::clear(static_cast<Signal*>(psig));psig = NULL;}
protected:
	void ptr(SignalTp *_psig){
		psig = _psig;use(static_cast<Signal*>(psig));
	}
private:
	mutable SignalTp *psig;
};

}

#endif
