/* Declarations file commandableobject.h
	
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

#ifndef CS_COMMANDABLE_OBJECT_H
#define CS_COMMANDABLE_OBJECT_H

#include <vector>
#include "command.h"
#include "cmdptr.h"
#include "common.h"

#include "system/cassert.h"

namespace clientserver{
//! A class for generic handeling of commands by clientserver::Object(s)
/*!
	<b>Usage</b><br>
	- You must directly inherit from CommandableObject
	- In your object's execute method, under object mutex's lock, 
		call grabCommands if you received S_CMD signal
	- After exiting the lock, call execCommands(*this)
*/
template <class B>
class CommandableObject: public B{
public:
	//!Comodity one parameter template constructor - forward to base
	template <typename T>
	CommandableObject(T _t):B(_t){}
	//!Comodity two parameters template constructor - forward to base
	template <typename T1, typename T2>
	CommandableObject(T1 _t1, T2 _t2):B(_t1,_t2){}
	/*virtual*/ int signal(CmdPtr<Command> &_cmd){
		if(this->state() < 0){
			_cmd.clear();
			return 0;//no reason to raise the pool thread!!
		}
		cmdv.push_back(_cmd);
		return B::signal(S_CMD | S_RAISE);
	}
	//! Grab the received commands into the run vector
	/*!
		Call this in your objects execute method, under object's mutex lock
	*/
	void grabCommands(){
		cassert(runv.empty());
		runv = cmdv; cmdv.clear();
	}
	
	//! Execute the commands from the run vector
	/*!
		Call this in your objects execute method, outside object's mutex lock
	*/
	template <class T>
	int execCommands(T &_thisobj){
		int rv = OK;
		typename CmdVecTp::iterator it(runv.begin());
		for(; it != runv.end(); ++it){
			switch(static_cast<typename T::CommandTp&>(*(*it)).execute(_thisobj)){
				case BAD: rv = BAD; goto done;
				case OK:  (*it).release(); break;
				case NOK: delete (*it).release(); break;
			}
		}
		done:
		runv.clear();
		return rv;
	}
	//! Clear both input and run vectors
	void clear(){
		cmdv.clear();runv.clear();
	}
	virtual ~CommandableObject(){ }
protected:
	typedef std::vector<CmdPtr<Command> >  CmdVecTp;
	CommandableObject(){}
	CmdVecTp    runv;
private:
	CmdVecTp    cmdv;
};

}//namespace


#endif
