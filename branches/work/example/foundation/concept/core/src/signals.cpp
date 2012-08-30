#include "example/foundation/concept/core/signals.hpp"
#include "example/foundation/concept/core/manager.hpp"
#include "foundation/signalexecuter.hpp"
#include "utility/iostream.hpp"
#include "system/debug.hpp"


namespace fdt=foundation;

namespace concept{

//----------------------------------------------------------------------
InputStreamSignal::InputStreamSignal(
	StreamPointer<InputStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int InputStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
OutputStreamSignal::OutputStreamSignal(
	StreamPointer<OutputStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int OutputStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
	_rce.sendSignal(_rthis_ptr, requid);
	return fdt::LEAVE;
}
//----------------------------------------------------------------------
InputOutputStreamSignal::InputOutputStreamSignal(
	StreamPointer<InputOutputStream> &_sptr,
	const FileUidT &_rfuid,
	const RequestUidT &_requid
):sptr(_sptr), fileuid(_rfuid), requid(_requid){
}
int InputOutputStreamSignal::execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidT &, TimeSpec &){
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
void ResolveDataSignal::init(
	const char *_node,
	int _port, 
	int _flags,
	int _family,
	int _type,
	int _proto
){
	resolvedata = synchronous_resolve(_node, _port, _flags, _family, _type, _proto);
}
void ResolveDataSignal::init(
	const char *_node,
	const char *_port, 
	int _flags,
	int _family,
	int _type,
	int _proto
){
	resolvedata = synchronous_resolve(_node, _port, _flags, _family, _type, _proto);
}

void ResolveDataSignal::result(const ObjectUidT &_rv){
}

}//namespace concept
