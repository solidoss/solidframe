/* Declarations file talkerselector.hpp
	
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

#ifndef CS_UDP_TALKERSELECTOR_HPP
#define CS_UDP_TALKERSELECTOR_HPP

#include "clientserver/core/common.hpp"
#include "clientserver/core/objptr.hpp"

namespace clientserver{
namespace udp{

class Talker;

typedef ObjPtr<Talker>	TalkerPtrTp;

//! A selector for tcp::Talker, that uses epoll on linux.
/*!
	It is designed to work with clientserver::SelectPool
*/
class TalkerSelector{
public:
	typedef TalkerPtrTp		ObjectTp;
	TalkerSelector();
	~TalkerSelector();
	int reserve(ulong _cp);
	//signal a specific object
	void signal(uint _pos = 0);
	void run();
	uint capacity()const;
	uint size() const;
	int  empty()const;
	int  full()const;
	
	void prepare();
	void unprepare();
	
	void push(const TalkerPtrTp &_rcon, uint _thid);
private://data
	struct Data;
	Data	&d;
};


}//namespace tcp
}//namespace clientserver

#endif

