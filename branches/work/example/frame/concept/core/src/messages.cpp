#include "example/frame/concept/core/messages.hpp"
#include "example/frame/concept/core/manager.hpp"
#include "frame/messagesteward.hpp"
#include "utility/iostream.hpp"
#include "system/debug.hpp"


using namespace solid;

namespace concept{

//----------------------------------------------------------------------
InputStreamMessage::InputStreamMessage(
	solid::StreamPointer<solid::InputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}

int InputStreamMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rms,
	const MessageUidT &,
	TimeSpec &
){
	_rms.sendMessage(_rmsgptr, requid);
	return AsyncError;
}
//----------------------------------------------------------------------
OutputStreamMessage::OutputStreamMessage(
	StreamPointer<OutputStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}

int OutputStreamMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rms,
	const MessageUidT &,
	TimeSpec &
){
	_rms.sendMessage(_rmsgptr, requid);
	return AsyncError;
}
//----------------------------------------------------------------------
InputOutputStreamMessage::InputOutputStreamMessage(
	StreamPointer<InputOutputStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}

int InputOutputStreamMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rms,
	const MessageUidT &,
	TimeSpec &
){
	_rms.sendMessage(_rmsgptr, requid);
	return AsyncError;
}
//----------------------------------------------------------------------
StreamErrorMessage::StreamErrorMessage(
	int _errid,
	const RequestUidT &_requid
):errid(_errid), requid(_requid){
}

int StreamErrorMessage::execute(
	DynamicPointer<Message> &_rmsgptr,
	uint32 _evs,
	frame::MessageSteward& _rms,
	const MessageUidT &,
	TimeSpec &
){
	_rms.sendMessage(_rmsgptr, requid);
	return AsyncError;
}
//----------------------------------------------------------------------
}//namespace concept
