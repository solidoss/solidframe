/* Declarations file alphawriter.h
	
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

#ifndef ALPHA_WRITER_H
#define ALPHA_WRITER_H

#include "algorithm/protocol/writer.h"

namespace clientserver{
namespace tcp{
class Channel;
}
}

namespace test{
namespace alpha{

class Writer: public protocol::Writer{
public:
	Writer(clientserver::tcp::Channel &rch);
	~Writer();
	void clear();
	static int putAString(protocol::Writer &_rw, protocol::Parameter &_rp);
	static int putStatus(protocol::Writer &_rw, protocol::Parameter &_rp);
	static int putCrlf(protocol::Writer &_rw, protocol::Parameter &_rp);
	static int clear(protocol::Writer &_rw, protocol::Parameter &_rp);
	template <class C>
	static int reinit(protocol::Writer &_rw, protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(static_cast<Writer&>(_rw), _rp);
	}
	protocol::String &message(){return msgs;}
	protocol::String &tag(){return tags;}
private:
	static int putQString(protocol::Writer &_rw, protocol::Parameter &_rp);
	/*virtual*/ int write(char *_pb, uint32 _bl);
	/*virtual*/ int write(IStreamIterator&_rit, uint64 _sz, char *_pb, uint32 _bl);
	//virtual int doManage(int _mo);
private:
	clientserver::tcp::Channel	&rch;
	protocol::String			msgs;
	protocol::String			tags;
};

}
}


#endif
