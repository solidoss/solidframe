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

class Connection: public clientserver::tcp::Connection{
public:
	typedef Command	CommandTp;
	typedef std::pair<uint32, uint32> FromPairTp;
	virtual ~Connection(){}
	virtual int receiveIStream(
		StreamPtr<IStream> &,
		const FromPairTp&_from = FromPairTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveOStream(
		StreamPtr<OStream> &,
		const FromPairTp&_from = FromPairTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveIOStream(
		StreamPtr<IOStream> &, 
		const FromPairTp&_from = FromPairTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	virtual int receiveString(
		const String &_str, 
		const FromPairTp&_from = FromPairTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
protected:
	Connection(clientserver::tcp::Channel *_pch):
			clientserver::tcp::Connection(_pch){}
};

}

#endif
