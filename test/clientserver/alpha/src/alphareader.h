/* Declarations file alphareader.h
	
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

#ifndef ALPHA_READER_H
#define ALPHA_READER_H

#include "algorithm/protocol/reader.h"
#include <string>


namespace clientserver{
namespace tcp{
class Channel;
}
}


namespace test{
namespace alpha{

class Writer;

class Reader: public protocol::Reader{
public:
	enum{
		QuotedString = LastBasicError
	};
	Reader(clientserver::tcp::Channel &_rch, Writer &_rw);
	~Reader();
	void clear();
	static int fetchAString(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int fetchQString(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int flushWriter(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int copyTmpString(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int checkLiteral(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int errorPrepare(protocol::Reader &_rr, protocol::Parameter &_rp);
	static int errorRecovery(protocol::Reader &_rr, protocol::Parameter &_rp);
	template <class C>
	static int reinit(protocol::Reader &_rr, protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitReader(static_cast<Reader&>(_rr), _rp);
	}
private:
	/*virtual*/ int read(char *_pb, uint32 _bl);
	/*virtual*/ int read(OStreamIterator &_rosi, uint64 _sz, char *_pb, uint32 _bl);
	/*virtual*/ int readSize()const;
	//virtual int doManage(int _mo);
	/*virtual*/ void prepareErrorRecovery();
	/*virtual*/ void charError(char _popc, char _expc);
	/*virtual*/ void keyError(const protocol::String &_pops, int _id = Unexpected);
	/*virtual*/ void basicError(int _id);
	int extractLiteralLength(uint32 &_litlen);
private:
	clientserver::tcp::Channel	&rch;
	Writer 						&rw;
};

}//namespace alpha
}//namespace test

#endif

