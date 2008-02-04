/* Declarations file talker.h
	
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

#ifndef CS_UDP_TALKER_H
#define CS_UDP_TALKER_H

#include "clientserver/core/object.h"

namespace clientserver{
namespace udp{
//! The base class for tcp connections
/*!
	<b>Usage:</b><br>
	- Inherit and implement execute for protocol logic.
	- Asynchrounously read/write data using given clientserver::udp::Station.
	- At run time insert created talkers into services and into specific SelectPools
	using clientserver::udp::TalkerSelector.
	
	<b>Notes:</b><br>
	- For a quite complex usage of the clientserver::udp::Talker see 
	clientserver::ipc::Talker.
*/

class Station;
class Talker: public Object{
public:
    virtual ~Talker();
    Station& station()const {return rst;}
    virtual int accept(clientserver::Visitor &_roi);
    virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
protected:
    Talker(Station *_pst);
private:
    //no copy construct or copy operator for this class
    Talker(const Talker &rt):rst(rt.rst){}
    Talker& operator=(const Talker&){return *this;}
private:
    Station             &rst;
};

}//namespace udp
}//namespace clientserver

#endif
