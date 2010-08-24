/* Declarations file alphareader.hpp
	
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

#ifndef ALPHA_READER_HPP
#define ALPHA_READER_HPP

#include "algorithm/protocol/reader.hpp"
#include <string>



namespace concept{
namespace alpha{
class Connection;
class Writer;
//! A reader better swited to alpha protocol needs.
/*!
	Extends the interface of the protocol::Reader, and implements
	the requested virtual methods.
*/
class Reader: public protocol::Reader{
public:
	enum{
		QuotedString = LastBasicError
	};
	Reader(Writer &_rw, protocol::Logger *_plog = NULL);
	~Reader();
	void clear();
	//! Asynchrounously reads an astring (atom/quoted/literal)
	static int fetchAString(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously reads an qstring
	static int fetchQString(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously flushes the writer
	static int flushWriter(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously copies the temp string to a given location
	static int copyTmpString(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously check for the type of the literal (plus or not)
	static int checkLiteral(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously prepares for error recovery
	static int errorPrepare(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Asynchrounously recovers from an error
	static int errorRecovery(protocol::Reader &_rr, protocol::Parameter &_rp);
	//! Local asynchrounous reinit method.
	template <class C>
	static int reinit(protocol::Reader &_rr, protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitReader(static_cast<Reader&>(_rr), _rp);
	}
	template <class C, int V>
	static int reinitExtended(protocol::Reader &_rr, protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->template reinitReader<V>(static_cast<Reader&>(_rr), _rp);
	}
private:
	/*virtual*/ int read(char *_pb, uint32 _bl);
	/*virtual*/ int readSize()const;
	//virtual int doManage(int _mo);
	/*virtual*/ void prepareErrorRecovery();
	/*virtual*/ void charError(char _popc, char _expc);
	/*virtual*/ void keyError(const String &_pops, int _id = Unexpected);
	/*virtual*/ void basicError(int _id);
	int extractLiteralLength(uint32 &_litlen);
private:
	Writer 		&rw;
};

}//namespace alpha
}//namespace concept

#endif

