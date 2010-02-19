#include "example/foundation/core/signals.hpp"
#include "example/foundation/core/manager.hpp"
#include "foundation/signalexecuter.hpp"
#include "utility/iostream.hpp"
#include "system/debug.hpp"


namespace fdt=foundation;

namespace concept{

//----------------------------------------------------------------------
IStreamSignal::IStreamSignal(
	StreamPointer<IStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int IStreamSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	DynamicPointer<Signal> cmdptr(this);
	_rce.sendSignal(cmdptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
OStreamSignal::OStreamSignal(
	StreamPointer<OStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int OStreamSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	DynamicPointer<Signal> cmdptr(this);
	_rce.sendSignal(cmdptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
IOStreamSignal::IOStreamSignal(
	StreamPointer<IOStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int IOStreamSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	DynamicPointer<Signal> cmdptr(this);
	_rce.sendSignal(cmdptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
StreamErrorSignal::StreamErrorSignal(
	int _errid,
	const RequestUidT &_requid
):errid(_errid), requid(_requid){
}
int StreamErrorSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	DynamicPointer<Signal> cmdptr(this);
	_rce.sendSignal(cmdptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
void AddrInfoSignal::init(
	const char *_node,
	int _port, 
	int _flags,
	int _family,
	int _type,
	int _proto
){
	addrinfo.reinit(_node, _port, _flags, _family, _type, _proto);
}

void AddrInfoSignal::result(int _rv){
}

}//namespace concept
