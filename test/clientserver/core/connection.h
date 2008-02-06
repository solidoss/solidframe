/* Declarations file connection.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TESTCONNECTION_H
#define TESTCONNECTION_H

#include "utility/streamptr.h"
#include "clientserver/tcp/connection.h"
#include "common.h"
#include "tstring.h"

class IStream;
class OStream;
class IOStream;

namespace clientserver{
namespace ipc{
struct ConnectorUid;
}
}
namespace test{
struct Command;
//! The base class for all connections knowing how to receive things
class Connection: public clientserver::tcp::Connection{
public:
	typedef Command	CommandTp;
	typedef std::pair<uint32, uint32>	FileUidTp;
	typedef std::pair<uint32, uint32>	RequestUidTp;
	//! Dummy method for receiving istreams
	/*!
		\param _sp Stream pointer
		\param _fuid The file unique id
		\param _requid The request uid as given by the requester of the stream
		\param _which Used for receiving more then one stream with a single command
		\param _from The object who has sent the stream
		\param _conid The ipc connection on which the stream was received
	*/
	virtual int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp	&,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveString(
		const String &_str,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveNumber(
		const int64 &_no,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveError(
		int _errid, 
		const RequestUidTp &_requid,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
protected:
	Connection(clientserver::tcp::Channel *_pch):
			clientserver::tcp::Connection(_pch){}
};

}

#endif
