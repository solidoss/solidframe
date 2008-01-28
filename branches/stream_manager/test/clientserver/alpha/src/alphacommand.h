/* Declarations file alphacommand.h
	
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

#ifndef ALPHA_COMMAND_H
#define ALPHA_COMMAND_H

#include "utility/streamptr.h"
#include "core/common.h"
#include "core/tstring.h"

class IStream;
class OStream;

namespace test{

class Server;

namespace alpha{

class Connection;
class Reader;

class Command{
protected:
	Command();
public:
	static void initStatic(Server &_rs);
	virtual ~Command();
	virtual void initReader(Reader &) = 0;
	virtual int execute(Connection &) = 0;
	//received from filemanager
	virtual int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	virtual int receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	virtual int receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp &,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	virtual int receiveString(
		const String &_str,
		int			_which, 
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	virtual int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	virtual int receiveError(
		int _errid,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);

};


}//namespace alpha
}//namespace test


#endif
