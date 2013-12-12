// alphawriter.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_WRITER_HPP
#define ALPHA_WRITER_HPP

#include "protocol/text/writer.hpp"
#include "core/tstring.hpp"

using solid::uint32;

namespace concept{
namespace alpha{
class Connection;
//! A writer better swited to alpha protocol needs.
/*!
	Extends the interface of the protocol::Writer, and implements
	the requested virtual methods.
*/
class Writer: public solid::protocol::text::Writer{
public:
	Writer(solid::protocol::text::Logger *_plog = NULL);
	~Writer();
	void clear();
	//! Asynchrounously writes an astring (atom/quoted/literal)
	static int putAString(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously writes the command's status (the last line of the command
	static int putStatus(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously writes a CRLF
	static int putCrlf(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp);
	//! Asynchrounously clears the writer data
	static int clear(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp);
	template <class C>
	static int reinit(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(static_cast<Writer&>(_rw), _rp);
	}
	solid::String &message(){return msgs;}
	solid::String &tag(){return tags;}
private:
	//! Asynchrounously writes a quoted string
	static int putQString(solid::protocol::text::Writer &_rw, solid::protocol::text::Parameter &_rp);
	/*virtual*/ int write(char *_pb, uint32 _bl);
	//virtual int doManage(int _mo);
private:
	solid::String		msgs;
	solid::String		tags;
};

}//namespace alpha
}//namespace concept


#endif
