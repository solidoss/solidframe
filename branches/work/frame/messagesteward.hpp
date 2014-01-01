// frame/messagesteward.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MESSAGE_STEWARD_HPP
#define SOLID_FRAME_MESSAGE_STEWARD_HPP

#include "frame/object.hpp"
#include "frame/common.hpp"

#include "utility/streampointer.hpp"
#include "utility/dynamicpointer.hpp"

#include <string>

namespace solid{
	
class InputStream;
class OutputStream;
class InputOutputStream;

struct TimeSpec;

namespace frame{

namespace ipc{
struct ConnectionUid;
}
//! An object which will only execute the received signals
/*!
	<b>Overview:</b><br>
	- received messages are given an unique id (used for example
		with file manager as request id).
	- execute the messages eventually repeatedly until the messages want to
		be destroyed or want to leave the steward.
	- also can execute a message when receiving something for that message
		(a stream from FileManager, another message, a string etc)
	- The _requid parameter of the receiveSomething methods, will uniquely
		identify the messages and must be the same with MessageUidT parameter
		of the Signal::execute(SignalExecuter&, const SignalUidT &, TimeSpec &_rts).
	
	<b>Usage:</b><br>
	TODO:...
*/
class MessageSteward: public Dynamic<MessageSteward, Object>{
public:
	MessageSteward();
	~MessageSteward();
	bool notify(DynamicPointer<Message> &_rmsgptr);
	void sendMessage(
		DynamicPointer<Message> &_rmsgptr,
		const RequestUidT &_requid,
		const ObjectUidT& _from = ObjectUidT(),
		const ipc::ConnectionUid *_conid = NULL
	);
private:
	void execute(ExecuteContext &_rexectx);
private:
	/*virtual*/ void init(Mutex*);
	void doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout);
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif
