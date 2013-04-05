/* Declarations file alphawriter.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ALPHA_WRITER_HPP
#define ALPHA_WRITER_HPP

#include "protocol/writer.hpp"
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
class Writer: public solid::protocol::Writer{
public:
	Writer(solid::protocol::Logger *_plog = NULL);
	~Writer();
	void clear();
	//! Asynchrounously writes an astring (atom/quoted/literal)
	static int putAString(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp);
	//! Asynchrounously writes the command's status (the last line of the command
	static int putStatus(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp);
	//! Asynchrounously writes a CRLF
	static int putCrlf(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp);
	//! Asynchrounously clears the writer data
	static int clear(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp);
	template <class C>
	static int reinit(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(static_cast<Writer&>(_rw), _rp);
	}
	String &message(){return msgs;}
	String &tag(){return tags;}
private:
	//! Asynchrounously writes a quoted string
	static int putQString(solid::protocol::Writer &_rw, solid::protocol::Parameter &_rp);
	/*virtual*/ int write(char *_pb, uint32 _bl);
	//virtual int doManage(int _mo);
private:
	String		msgs;
	String		tags;
};

}//namespace alpha
}//namespace concept


#endif
