// alphareader.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_READER_HPP
#define ALPHA_READER_HPP

#include "protocol/text/reader.hpp"
#include "core/tstring.hpp"

using solid::uint32;


namespace concept{
namespace alpha{
class Connection;
class Writer;
//! A reader better swited to alpha protocol needs.
/*!
	Extends the interface of the protocol::Reader, and implements
	the requested virtual methods.
*/
class Reader: public solid::protocol::text::Reader{
public:
	enum{
		QuotedString = LastBasicError
	};
	Reader(Writer &_rw, solid::protocol::text::Logger *_plog = NULL);
	~Reader();
	void clear();
	//! Asynchrounously reads an astring (atom/quoted/literal)
	static int fetchAString(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously reads an qstring
	static int fetchQString(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously flushes the writer
	static int flushWriter(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously copies the temp string to a given location
	static int copyTmpString(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously check for the type of the literal (plus or not)
	static int checkLiteral(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously prepares for error recovery
	static int errorPrepare(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously recovers from an error
	static int errorRecovery(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp);
	//! Local asynchrounous reinit method.
	template <class C>
	static int reinit(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitReader(static_cast<Reader&>(_rr), _rp);
	}
	template <class C, int V>
	static int reinitExtended(solid::protocol::text::Reader &_rr, solid::protocol::text::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->template reinitReader<V>(static_cast<Reader&>(_rr), _rp);
	}
private:
	/*virtual*/ int read(char *_pb, uint32 _bl);
	/*virtual*/ int readSize()const;
	//virtual int doManage(int _mo);
	/*virtual*/ void prepareErrorRecovery();
	/*virtual*/ void charError(char _popc, char _expc);
	/*virtual*/ void keyError(const solid::String &_pops, int _id = Unexpected);
	/*virtual*/ void basicError(int _id);
	int extractLiteralLength(uint32 &_litlen);
private:
	Writer 		&rw;
};

}//namespace alpha
}//namespace concept

#endif

