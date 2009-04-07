/* Declarations file command.hpp
	
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

#ifndef CS_COMMAND_HPP
#define CS_COMMAND_HPP

#include "foundation/common.hpp"
#include "utility/streamptr.hpp"
#include <string>

class IStream;
class OStream;
class IOStream;
class TimeSpec;

namespace foundation{
namespace ipc{
struct ConnectorUid;
struct CommandUid;
}
template <class T>
class CmdPtr;
class CommandExecuter;
class Object;
//! A base class for commands to be sent to objects
/*!
	Inherit this class if you want so send something to an object.
	Implement the proper execute method.
	Also the command can be used with an foundation::CommandExecuter,
	in which case the posibilities widen.
	\see test::alpha::FetchMasterCommand
*/
struct Command{
	Command();
	virtual ~Command();
	//! Called by ipc module after the command was successfully parsed
	/*!
		\retval BAD for deleting the command, OK for not
	*/
	virtual int ipcReceived(ipc::CommandUid &, const ipc::ConnectorUid&);
	//! Called by ipc module, before the command begins to be serialized
	virtual int ipcPrepare(const ipc::CommandUid&);
	//! Called by ipc module on peer failure detection (disconnect,reconnect)
	virtual void ipcFail(int _err);
	//! Used by CmdPtr - smartpointers
	virtual void use();
	//! Execute the command only knowing its for an object
	virtual int execute(Object &);
	//! Execute the command knowing its for a CommandExecuter
	virtual int execute(uint32 _evs, CommandExecuter&, const CommandUidTp &, TimeSpec &_rts);
	//! Used by CmdPtr to know if the command must be deleted
	virtual int release();
	//! Used with CommandExecuter to receive a command
	virtual int receiveCommand(
		CmdPtr<Command> &_rcmd,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive an istream
	virtual int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp	&,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive an ostream
	virtual int receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp	&,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive an iostream
	virtual int receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp	&,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive a string
	virtual int receiveString(
		const std::string &_str,
		int	 _which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive a number
	virtual int receiveNumber(
		const int64 &_no,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
	//! Used with CommandExecuter to receive an error
	virtual int receiveError(
		int _errid, 
		const ObjectUidTp&_from = ObjectUidTp(),
		const ipc::ConnectorUid *_conid = NULL
	);
};
}//namespace foundation

#endif
