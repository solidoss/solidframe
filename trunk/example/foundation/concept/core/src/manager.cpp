/* Implementation file manager.cpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#include <map>
#include <vector>

#include "system/debug.hpp"

#include "utility/iostream.hpp"
#include "utility/dynamictype.hpp"

#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"

#include "core/manager.hpp"
#include "core/service.hpp"
#include "core/signals.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aioobject.hpp"

#include "foundation/objectselector.hpp"
#include "foundation/signalexecuter.hpp"
#include "foundation/signal.hpp"
#include "foundation/requestuid.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/file/filemanager.hpp"
#include "foundation/file/filemappers.hpp"

#include <iostream>
using namespace std;

//some forward declarations
namespace foundation{

class ObjectSelector;

namespace aio{
class Selector;
}
}//namespace foundation

namespace fdt=foundation;


namespace concept{
//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------

class FileManagerController: public fdt::file::Manager::Controller{
public:
	FileManagerController(){}
protected:
	/*virtual*/ void init(const fdt::file::Manager::InitStub &_ris);
	 
	/*virtual*/ bool release();
	
	/*virtual*/ void sendStream(
		StreamPointer<IStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<OStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<IOStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendError(
		int _error,
		const fdt::RequestUid& _rrequid
	);
};


//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------


struct IpcServiceController: foundation::ipc::Service::Controller{
	/*virtual*/ void scheduleTalker(foundation::aio::Object *_po);
	/*virtual*/ bool release();
};

//------------------------------------------------------
//		Manager::Data
//------------------------------------------------------

namespace{
	FileManagerController	fmctrl;
	IpcServiceController	ipcctrl;
}

struct Manager::Data{
	Data():filemanageridx(1), readsigexeidx(2), writesigexeidx(3), ipcidx(4){}
	const IndexT			filemanageridx;
	const IndexT			readsigexeidx;
	const IndexT			writesigexeidx;
	const IndexT			ipcidx;
};

//--------------------------------------------------------------------------
typedef serialization::TypeMapper					TypeMapper;
typedef serialization::IdTypeMap					IdTypeMap;
typedef serialization::bin::Serializer				BinSerializer;

Manager::Manager():foundation::Manager(16), d(*(new Data())){
	//NOTE: Use the following line instead of ThisGuard if you only have one Manager per process, else use the ThisGuard for any function
	// that may be called from a thread that has access to other managers.
	//this->prepareThread();
	ThisGuard	tg(this);
	
	TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);
	TypeMapper::registerSerializer<BinSerializer>();
	
	registerScheduler(new SchedulerT(*this));
	registerScheduler(new AioSchedulerT(*this));
	registerScheduler(new AioSchedulerT(*this));
	
	registerObject<SchedulerT>(new fdt::file::Manager(&fmctrl), 0, d.filemanageridx);
	registerObject<SchedulerT>(new fdt::SignalExecuter, 0, d.readsigexeidx);
	registerObject<SchedulerT>(new fdt::SignalExecuter, 0, d.writesigexeidx);
	registerService<SchedulerT>(new foundation::ipc::Service(&ipcctrl), 0, d.ipcidx);
}

Manager::~Manager(){
	delete &d;
}

ObjectUidT Manager::readSignalExecuterUid()const{
	return object(d.readsigexeidx).uid();
}
	
ObjectUidT Manager::writeSignalExecuterUid()const{
	return object(d.writesigexeidx).uid();
}

foundation::ipc::Service&	Manager::ipc()const{
	return static_cast<foundation::ipc::Service&>(foundation::Manager::service(d.ipcidx));
}
foundation::file::Manager&	Manager::fileManager()const{
	return static_cast<foundation::file::Manager&>(foundation::Manager::object(d.filemanageridx));
}
void Manager::signalService(const IndexT &_ridx, DynamicPointer<foundation::Signal> &_rsig){
	signal(_rsig, serviceUid(_ridx));
}

//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
void IpcServiceController::scheduleTalker(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 1);
}
bool IpcServiceController::release(){
	return false;
}
//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------
/*virtual*/ void FileManagerController::init(const fdt::file::Manager::InitStub &_ris){
	_ris.registerMapper(new fdt::file::NameMapper(10, 0));
	_ris.registerMapper(new fdt::file::TempMapper(1024 * 1024 * 1024, "/tmp"));
	_ris.registerMapper(new fdt::file::MemoryMapper(1024 * 1024 * 100));
}
/*virtual*/ bool FileManagerController::release(){
	return false;
}
void FileManagerController::sendStream(
	StreamPointer<IStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new IStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}

void FileManagerController::sendStream(
	StreamPointer<OStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new OStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}
void FileManagerController::sendStream(
	StreamPointer<IOStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new IOStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}
void FileManagerController::sendError(
	int _error,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new StreamErrorSignal(_error, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}



}//namespace concept

