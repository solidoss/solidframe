#ifndef CONCEPT_GAMMA_COMMAND_HPP
#define CONCEPT_GAMMA_COMMAND_HPP

#include "utility/streampointer.hpp"
#include "system/specific.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"

class IStream;
class OStream;

namespace concept{

class Manager;

namespace gamma{

class Connection;
class Reader;

//! A base class for all alpha protocol commands
class Command: public SpecificObject{
protected:
	Command();
public:
	//! Initiate static data for all commands - like registering serializable structures.
	static void initStatic(Manager &_rm);
	//! Virtual destructor
	virtual ~Command();
	//! Called by the reader to learn how to parse the command
	virtual void initReader(Reader &) = 0;
	//! Called by alpha::Connection to prepare the response
	virtual int execute(const uint _sid) = 0;
	
	virtual void contextData(ObjectUidT &_robjuid);
	//received from filemanager
	//! Receive an istream
	virtual int receiveIStream(
		StreamPointer<IStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive an ostream
	virtual int receiveOStream(
		StreamPointer<OStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive an iostream
	virtual int receiveIOStream(
		StreamPointer<IOStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive a string
	virtual int receiveString(
		const String &_str,
		int			_which, 
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive data
	virtual int receiveData(
		void *_pdata,
		int _datasz,
		int			_which, 
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive a number
	virtual int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);
	//! Receive an error code
	virtual int receiveError(
		int _errid,
		const ObjectUidT&_from,
		const foundation::ipc::ConnectionUid *_conid
	);

};

}//namespace gamma
}//namespace concept


#endif
