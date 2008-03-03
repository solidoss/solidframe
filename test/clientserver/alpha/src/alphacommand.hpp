/* Declarations file alphacommand.hpp
	
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

#ifndef ALPHA_COMMAND_H
#define ALPHA_COMMAND_H

#include "utility/streamptr.hpp"
#include "core/common.hpp"
#include "core/tstring.hpp"

class IStream;
class OStream;

namespace test{

class Server;

namespace alpha{

class Connection;
class Reader;
//! A base class for all alpha protocol commands
class Command{
protected:
	Command();
public:
	//! Initiate static data for all commands - like registering serializable structures.
	static void initStatic(Server &_rs);
	//! Virtual destructor
	virtual ~Command();
	//! Called by the reader to learn how to parse the command
	virtual void initReader(Reader &) = 0;
	//! Called by alpha::Connection to prepare the response
	virtual int execute(Connection &) = 0;
	//received from filemanager
	//! Receive an istream
	virtual int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	//! Receive an ostream
	virtual int receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	//! Receive an iostream
	virtual int receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	//! Receive a string
	virtual int receiveString(
		const String &_str,
		int			_which, 
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	//! Receive a number
	virtual int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	//! Receive an error code
	virtual int receiveError(
		int _errid,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);

};


}//namespace alpha
}//namespace test


#endif
