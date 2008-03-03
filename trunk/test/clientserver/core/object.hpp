/* Declarations file object.hpp
	
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

#ifndef TESTOBJECT_H
#define TESTOBJECT_H

#include "utility/streamptr.hpp"
#include "clientserver/core/object.hpp"
#include "common.hpp"
#include "tstring.hpp"

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

//! A test variant of clientserver::Object
/*!
	For now all it knows is to receive things.
*/
class Object: public clientserver::Object{
public:
	typedef Command	CommandTp;
	typedef std::pair<uint32, uint32>	FromPairTp;
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
		StreamPtr<IStream> &_sp,
		const FileUidTp	&_fuid,
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
	virtual int receiveError(
		int _errid, 
		const RequestUidTp &_requid,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
protected:
	Object(uint32 _fullid = 0):clientserver::Object(_fullid){}
};

}

#endif
