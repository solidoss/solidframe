/* Declarations file messagesteward.hpp
	
	Copyright 2013 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

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
	int execute(ulong _evs, TimeSpec &_rtout);
	void sendMessage(
		DynamicPointer<Message> &_rmsgptr,
		const RequestUidT &_requid,
		const ObjectUidT& _from = ObjectUidT(),
		const ipc::ConnectionUid *_conid = NULL
	);
private:
	/*virtual*/ void init(Mutex*);
	int execute();
	void doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout);
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif
