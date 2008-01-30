/* Declarations file object.h
	
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

#ifndef TESTOBJECT_H
#define TESTOBJECT_H

#include "utility/streamptr.h"
#include "clientserver/core/object.h"
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

class Object: public clientserver::Object{
public:
	typedef Command	CommandTp;
	typedef std::pair<uint32, uint32>	FromPairTp;
	typedef std::pair<uint32, uint32>	FileUidTp;
	typedef std::pair<uint32, uint32>	RequestUidTp;
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
