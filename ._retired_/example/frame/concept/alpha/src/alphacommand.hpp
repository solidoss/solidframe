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
struct FilePointerMessage;

namespace alpha{
struct FetchSlaveMessage;
struct RemoteListMessage;
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
	virtual void execute(Connection &) = 0;
	//received from filemanager
	//! Receive an istream
	virtual int receiveFilePointer(
		FilePointerMessage &_rmsg
	);
	virtual int receiveMessage(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr);
	virtual int receiveMessage(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);

};


}//namespace alpha
}//namespace concept


#endif
