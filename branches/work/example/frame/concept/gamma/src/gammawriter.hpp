#ifndef CONCEPT_GAMMA_WRITER_HPP
#define CONCEPT_GAMMA_WRITER_HPP

#include "protocol/text/writer.hpp"
#include "core/tstring.hpp"

using solid::uint;
using solid::uint32;

namespace concept{
namespace gamma{

class Connection;

//! A writer better swited to alpha protocol needs.
/*!
	Extends the interface of the solid::protocol::text::Writer, and implements
	the requested virtual methods.
*/
class Writer: public solid::protocol::text::Writer{
public:
	enum{
		Leave = LastReturnValue + 1,
	};
	Writer(uint _sid, solid::protocol::text::Logger *_plog = NULL);
	~Writer();
	uint socketId()const{return sid;}
	void socketId(const uint _sid){sid = _sid;}
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
	uint				sid;
	solid::String		msgs;
	solid::String		tags;
};

}//namespace gamma
}//namespace concept

#endif
