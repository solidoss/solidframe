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
