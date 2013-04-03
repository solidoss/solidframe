/* Declarations file messages.hpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
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

#ifndef CONCEPT_CORE_MESSAGES_HPP
#define CONCEPT_CORE_MESSAGES_HPP

#include "system/socketaddress.hpp"
#include "utility/streampointer.hpp"
#include "frame/message.hpp"
#include "common.hpp"

namespace solid{
class InputStream;
class OutputStream;
class InputOutputStream;
}//namespace solid

namespace concept{
//!	A signal for sending istreams from the fileManager
struct InputStreamMessage: solid::Dynamic<InputStreamMessage, solid::frame::Message>{
	InputStreamMessage(
		solid::StreamPointer<solid::InputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUidT &_requid
	);
	int execute(
		solid::DynamicPointer<Message> &_rmsgptr,
		solid::uint32 _evs,
		solid::frame::MessageSteward&,
		const MessageUidT &,
		solid::TimeSpec &
	);
	solid::StreamPointer<solid::InputStream>	sptr;
	FileUidT									fileuid;
	RequestUidT				requid;
};

//!A signal for sending ostreams from the fileManager
struct OutputStreamMessage: solid::Dynamic<OutputStreamMessage, solid::frame::Message>{
	OutputStreamMessage(
		solid::StreamPointer<solid::OutputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUidT &_requid
	);
	int execute(
		solid::DynamicPointer<solid::frame::Message> &_rmsgptr,
		solid::uint32 _evs,
		solid::frame::MessageSteward&,
		const MessageUidT &,
		solid::TimeSpec &
	);
	solid::StreamPointer<solid::OutputStream>	sptr;
	FileUidT									fileuid;
	RequestUidT									requid;
};


//!A signal for sending iostreams from the fileManager
struct InputOutputStreamMessage: solid::Dynamic<InputOutputStreamMessage, solid::frame::Message>{
	InputOutputStreamMessage(
		solid::StreamPointer<solid::InputOutputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUidT &_requid
	);
	int execute(
		solid::DynamicPointer<solid::frame::Message> &_rmsgptr,
		solid::uint32 _evs,
		solid::frame::MessageSteward&,
		const MessageUidT &,
		solid::TimeSpec &
	);
	
	solid::StreamPointer<solid::InputOutputStream>	sptr;
	FileUidT										fileuid;
	RequestUidT										requid;
};


//!A signal for sending errors from the fileManager
struct StreamErrorMessage: solid::Dynamic<StreamErrorMessage, solid::frame::Message>{
	StreamErrorMessage(
		int _errid,
		const RequestUidT &_requid
	);
	int execute(
		solid::DynamicPointer<solid::frame::Message> &_rmsgptr,
		solid::uint32 _evs,
		solid::frame::MessageSteward&,
		const SignalUidT &,
		solid::TimeSpec &
	);
	
	int				errid;
	RequestUidT		requid;
};

}//namespace concept


#endif
