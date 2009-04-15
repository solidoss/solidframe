/* Declarations file signalableobject.hpp
	
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

#ifndef CS_SIGNALABLE_OBJECT_HPP
#define CS_SIGNALABLE_OBJECT_HPP

#include <vector>
#include "signal.hpp"
#include "signalpointer.hpp"
#include "common.hpp"

#include "system/cassert.hpp"

namespace foundation{
//! A class for generic handeling of signals by foundation::Object(s)
/*!
	<b>Usage</b><br>
	- You must directly inherit from SignalableObject
	- In your object's execute method, under object mutex's lock, 
		call grabSignals if you received S_SIG signal
	- After exiting the lock, call execSignals(*this)
*/
template <class B>
class SingnalableObject: public B{
public:
	//!Comodity one parameter template constructor - forward to base
	template <typename T>
	SingnalableObject(const T &_t):B(_t){}
	//!Comodity two parameters template constructor - forward to base
	template <typename T1, typename T2>
	SingnalableObject(T1 _t1, T2 _t2):B(_t1,_t2){}
	/*virtual*/ int signal(SignalPointer<Signal> &_sig){
		if(this->state() < 0){
			_sig.clear();
			return 0;//no reason to raise the pool thread!!
		}
		sigv.push_back(_sig);
		return B::signal(S_SIG | S_RAISE);
	}
	//! Grab the received signals into the run vector
	/*!
		Call this in your objects execute method, under object's mutex lock
	*/
	void grabSignals(){
		cassert(runv.empty());
		runv = sigv; sigv.clear();
	}
	
	//! Execute the signals from the run vector
	/*!
		Call this in your objects execute method, outside object's mutex lock
	*/
	template <class T>
	int execSignals(T &_thisobj){
		int rv = OK;
		typename SigVecTp::iterator it(runv.begin());
		for(; it != runv.end(); ++it){
			//TODO:
// 			switch(static_cast<typename T::CommandTp&>(*(*it)).execute(_thisobj)){
// 				case BAD: rv = BAD; goto done;
// 				case OK:  (*it).release(); break;
// 				case NOK: delete (*it).release(); break;
// 			}
		}
		done:
		runv.clear();
		return rv;
	}
	//! Clear both input and run vectors
	void clear(){
		sigv.clear();runv.clear();
	}
	~SingnalableObject(){ clear();}
protected:
	typedef std::vector<CmdPtr<Command> >  SigVecTp;
	SingnalableObject(){}
	SigVecTp    runv;
private:
	SigVecTp    sigv;
};

}//namespace


#endif
