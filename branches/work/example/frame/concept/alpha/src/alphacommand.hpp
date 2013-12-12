// alphacommands.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_COMMAND_HPP
#define ALPHA_COMMAND_HPP

#include "utility/streampointer.hpp"
#include "core/common.hpp"
#include "core/tstring.hpp"
#include "system/specific.hpp"

namespace solid{
class InputStream;
class OutputStream;
}

using solid::int64;
using solid::uint64;

namespace concept{

class Manager;

namespace alpha{

class Connection;
class Reader;
//! A base class for all alpha protocol commands
class Command: public solid::SpecificObject{
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
	virtual int execute(Connection &) = 0;
	//received from filemanager
	//! Receive an istream
	virtual int receiveInputStream(
		solid::StreamPointer<solid::InputStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive an ostream
	virtual int receiveOutputStream(
		solid::StreamPointer<solid::OutputStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive an iostream
	virtual int receiveInputOutputStream(
		solid::StreamPointer<solid::InputOutputStream> &,
		const FileUidT &,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive a string
	virtual int receiveString(
		const solid::String &_str,
		int			_which, 
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive data
	virtual int receiveData(
		void *_pdata,
		int _datasz,
		int			_which, 
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive a number
	virtual int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	//! Receive an error code
	virtual int receiveError(
		int _errid,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);

};


}//namespace alpha
}//namespace concept


#endif
