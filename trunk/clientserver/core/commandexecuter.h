#ifndef CS_COMMAND_EXECUTER_H
#define CS_COMMAND_EXECUTER_H

#include "clientserver/core/object.h"
#include "clientserver/core/common.h"
#include "cmdptr.h"
#include "utility/streamptr.h"
#include <string>

class IStream;
class OStream;
class IOStream;


namespace clientserver{
namespace ipc{
struct ConnectorUid;
}
//! An object which will only execute the received commands
/*!
	<b>Overview:</b><br>
	- received commands are given an unique id (used for example
		with file manager as request id).
	- execute the commands eventually repeatedly until the commands want to 
		be destroyed or want to leave the executer.
	- also can execute a command when receiving something for that command
		(a stream from FileManager, another command, a string etc)
	- The _requid parameter of the receiveSomething methods, will uniquely
		identify the commands and must be the same with CommandUidTp parameter
		of the Command::execute(CommandExecuter&, const CommandUidTp &, TimeSpec &_rts).
	- The receiveSomething methods are forwards to the actual commnads identified by
		_requid parameter.
	
	<b>Usage:</b><br>
	- Inherit from CommandExecuter and implement removeFromServer
		in which you should call Server::the().removeObject(this);
	- In your server, create some commandexecuters and register them
		using clientserver::Server::insertObject
	- Implement for your commands execute(CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	
	\see test/clientserver/core/src/server.cpp test/clientserver/alpha/src/alphacommands.cpp
	\see test::CommandExecuter test::alpha::FetchMasterCommand
	\see clientserver::Command clientserver::Object
*/
class CommandExecuter: public Object{
public:
	CommandExecuter();
	~CommandExecuter();
	int signal(CmdPtr<Command> &_cmd);
	int execute(ulong _evs, TimeSpec &_rtout);
	virtual void removeFromServer() = 0;
	void mutex(Mutex *_pmut);
	void receiveCommand(
		CmdPtr<Command> &_rcmd,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveString(
		const std::string &_str,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveNumber(
		const int64 &_no,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	void receiveError(
		int _errid, 
		const RequestUidTp &_requid,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
private:
	int execute();
	struct Data;
	Data	&d;
};

}//namespace clientserver

#endif
