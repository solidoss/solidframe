/* Declarations file commandexecuter.hpp
	
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

#ifndef CS_COMMAND_EXECUTER_HPP
#define CS_COMMAND_EXECUTER_HPP

#include "foundation/core/object.hpp"
#include "foundation/core/common.hpp"
#include "cmdptr.hpp"
#include "utility/streamptr.hpp"
#include <string>

class IStream;
class OStream;
class IOStream;

struct TimeSpec;

namespace foundation{
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
	- Inherit from CommandExecuter and implement removeFromManager
		in which you should call Manager::the().removeObject(this);
	- In your server, create some commandexecuters and register them
		using foundation::Manager::insertObject
	- Implement for your commands execute(CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	
	\see test/foundation/core/src/server.cpp test/foundation/alpha/src/alphacommands.cpp
	\see test::CommandExecuter test::alpha::FetchMasterCommand
	\see foundation::Command foundation::Object
*/
class CommandExecuter: public Object{
public:
	CommandExecuter();
	~CommandExecuter();
	int signal(CmdPtr<Command> &_cmd);
	int execute(ulong _evs, TimeSpec &_rtout);
	virtual void removeFromManager() = 0;
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
	void doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout);
	struct Data;
	Data	&d;
};

}//namespace foundation

#endif
