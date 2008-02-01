/* Declarations file activeset.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_ACTIVESET_H
#define CS_ACTIVESET_H

namespace clientserver{
//! This is the base class for all active containers/sets (i.e. WorkPools)
/*!
	It is used by clientserver::Server for signaling a workpool
	currently holding an object.
*/
class ActiveSet{
public:
	ActiveSet(){}
	virtual ~ActiveSet(){}
	//! Wake a theread given by its index
	virtual void raise(uint _thridx) = 0;
	//! Wake an object
	/*!
		Wake an object given the thread on which it resides and an index in the 
		thread.
	*/
	virtual void raise(uint _thridx, uint _objid) = 0;
	//! Sets the pool's id, which in turn will be used for computing the thread id
	/*!
		This is called by the clientserver::Server on activeset's registration:
		clientserver::Server::registerActiveSet
	*/
	virtual void poolid(uint _id) =  0;
};

}//namespace

#endif

