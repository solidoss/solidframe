#ifndef CONCEPT_GAMMA_WRITER_HPP
#define CONCEPT_GAMMA_WRITER_HPP

#include "algorithm/protocol/writer.hpp"

namespace concept{
namespace gamma{

class Connection;

//! A writer better swited to alpha protocol needs.
/*!
	Extends the interface of the protocol::Writer, and implements
	the requested virtual methods.
*/
class Writer: public protocol::Writer{
public:
	enum{
		Leave = LastReturnValue + 1
	};
	Writer(uint _sid, protocol::Logger *_plog = NULL);
	~Writer();
	uint socketId()const{return sid;}
	void socketId(const uint _sid){sid = _sid;}
	void clear();
	//! Asynchrounously writes an astring (atom/quoted/literal)
	static int putAString(protocol::Writer &_rw, protocol::Parameter &_rp);
	//! Asynchrounously writes the command's status (the last line of the command
	static int putStatus(protocol::Writer &_rw, protocol::Parameter &_rp);
	//! Asynchrounously writes a CRLF
	static int putCrlf(protocol::Writer &_rw, protocol::Parameter &_rp);
	//! Asynchrounously clears the writer data
	static int clear(protocol::Writer &_rw, protocol::Parameter &_rp);
	template <class C>
	static int reinit(protocol::Writer &_rw, protocol::Parameter &_rp){
		return reinterpret_cast<C*>(_rp.a.p)->reinitWriter(static_cast<Writer&>(_rw), _rp);
	}
	String &message(){return msgs;}
	String &tag(){return tags;}
private:
	//! Asynchrounously writes a quoted string
	static int putQString(protocol::Writer &_rw, protocol::Parameter &_rp);
	/*virtual*/ int write(char *_pb, uint32 _bl);
	//virtual int doManage(int _mo);
private:
	uint		sid;
	String		msgs;
	String		tags;
};

}//namespace gamma
}//namespace concept

#endif
