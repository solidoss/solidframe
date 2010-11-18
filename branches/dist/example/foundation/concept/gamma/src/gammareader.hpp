
#ifndef CONCEPT_GAMMA_READER_HPP
#define CONCEPT_GAMMA_READER_HPP

#include "algorithm/protocol/reader.hpp"
#include <string>



namespace concept{
namespace gamma{

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
		QuotedString = LastBasicError,
		Idle = LastReturnValue + 1
	};
	uint socketId()const{return sid;}
	void socketId(const uint _sid){sid = _sid;}
	Reader(uint _sid, Writer &_rw, protocol::Logger *_plog = NULL);
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
	uint		sid;
	Writer 		&rw;
};

}//namespace gamma
}//namespace concept

#endif

