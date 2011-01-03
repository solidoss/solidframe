#ifndef CONCEPT_GAMMA_COMMANDS_HPP
#define CONCEPT_GAMMA_COMMANDS_HPP

#include <list>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "utility/queue.hpp"
#include "utility/streampointer.hpp"
#include "utility/istream.hpp"
#include "utility/ostream.hpp"

#include "foundation/ipc/ipcconnectionuid.hpp"

#include "core/common.hpp"

#include "gammacommand.hpp"

namespace fs = boost::filesystem;
using boost::filesystem::path;
using boost::next;
using boost::prior;


namespace protocol{
struct Parameter;
}

namespace concept{
namespace gamma{

class Connection;
class Reader;
class Writer;

//! Basic alpha commands - with no parameters.
class Basic: public Command{
public:
	enum Types{
		Noop, Logout, Capability
	};
	Basic(Types _tp);
	~Basic();
	void initReader(Reader &);
	int execute(const uint _sid);
private:
	int execNoop(const uint _sid);
	int execLogout(const uint _sid);
	int execCapability(const uint _sid);
private:
	Types tp;
};

//! Alpha Login command
/*!
	Syntax:<br>
	tag SP LOGIN SP astring = nume SP astring = password CRLF<br>
*/
class Login: public Command{
public:
	Login();
	~Login();
	void initReader(Reader &);
	int execute(const uint _sid);
private:
	void contextData(ObjectUidT &);
	String user,pass, ctx;
};

class Open: public Command{
public:
	Open();
	~Open();
	void initReader(Reader &);
	int execute(const uint _sid);
	
	int reinitWriter(Writer &_rw, protocol::Parameter &_rp);
private:
	int doInitLocal(const uint _sid);
	int doDoneLocal(Writer &_rw);
	int receiveIStream(
		StreamPointer<IStream> &_sptr,
		const FileUidT &_fuid,
		int _which,
		const ObjectUidT&,
		const foundation::ipc::ConnectionUid *
	);
	int receiveError(
		int _errid,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *
	);
private:
	enum{
		InitLocal,
		DoneLocal,
		WaitLocal,
		SendError
	};
	uint16					state;
	protocol::Parameter 	*pp;
	String					path;
	String					flags;
	uint32					reqid;
	StreamPointer<IStream>	isp;
	StreamPointer<IOStream>	iosp;
	StreamPointer<OStream>	osp;
};

}//namespace gamma
}//namespace concept

#endif

