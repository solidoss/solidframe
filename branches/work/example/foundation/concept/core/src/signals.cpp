#include "example/foundation/concept/core/signals.hpp"
#include "example/foundation/concept/core/manager.hpp"
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
int IStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
OStreamSignal::OStreamSignal(
	StreamPointer<OStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int OStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
IOStreamSignal::IOStreamSignal(
	StreamPointer<IOStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int IOStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
StreamErrorSignal::StreamErrorSignal(
	int _errid,
	const RequestUidT &_requid
):errid(_errid), requid(_requid){
}
int StreamErrorSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
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
