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

class ActiveSet{
public:
	ActiveSet(){}
	virtual ~ActiveSet(){}
	virtual void raise(uint _thridx) = 0;
	virtual void raise(uint _thridx, uint _objid) = 0;
	virtual void poolid(uint _id) =  0;
};

}//namespace

#endif

