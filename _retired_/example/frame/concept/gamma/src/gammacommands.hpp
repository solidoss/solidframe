#ifndef CONCEPT_GAMMA_COMMANDS_HPP
#define CONCEPT_GAMMA_COMMANDS_HPP

#include <list>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "utility/queue.hpp"

#include "frame/ipc/ipcconnectionuid.hpp"

#include "core/common.hpp"

#include "gammacommand.hpp"

namespace fs = boost::filesystem;
using boost::filesystem::path;
using boost::next;
using boost::prior;

using solid::uint16;

namespace solid{
namespace protocol{
struct Parameter;
}
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
	void execute(const uint _sid);
private:
	void execNoop(const uint _sid);
	void execLogout(const uint _sid);
	void execCapability(const uint _sid);
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
	void execute(const uint _sid);
private:
	void contextData(ObjectUidT &);
	solid::String user,pass, ctx;
};

}//namespace gamma
}//namespace concept

#endif

