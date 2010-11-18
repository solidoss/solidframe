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
#include "core/visitor.hpp"
#include "core/signals.hpp"
#include "core/connection.hpp"
#include "core/object.hpp"


#include "foundation/selectpool.hpp"
#include "foundation/execpool.hpp"

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

//======= FileControler:==================================================
/*
	Local implementation of the foundation::FileManager wich will now know how
	to send streams/errors to an object.
*/
class FileManagerController: public fdt::file::Manager::Controller{
public:
	FileManagerController(){}
protected:
	/*virtual*/ void init(const fdt::file::Manager::InitStub &_ris);
	/*virtual*/ void removeFileManager();
	
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

/*virtual*/ void FileManagerController::init(const fdt::file::Manager::InitStub &_ris){
	_ris.registerMapper(new fdt::file::NameMapper(10, 0));
	_ris.registerMapper(new fdt::file::TempMapper(1024 * 1024 * 1024, "/tmp"));
	_ris.registerMapper(new fdt::file::MemoryMapper(1024 * 1024 * 100));
}
/*virtual*/ void FileManagerController::removeFileManager(){
	Manager::the().removeFileManager();
}

void FileManagerController::sendStream(
	StreamPointer<IStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new IStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}

void FileManagerController::sendStream(
	StreamPointer<OStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new OStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}
void FileManagerController::sendStream(
	StreamPointer<IOStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new IOStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}
void FileManagerController::sendError(
	int _error,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new StreamErrorSignal(_error, ru));
	Manager::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}

//======= IpcService ======================================================
/*
	Local implementation of the ipc service which will know in which 
	pool to push the ipc talkers.
	This implementation allows for having some common code with the concept::Service, and to permit
	adding a talker using a signal.
	The recomended way is to just inherit from ipc::Service, and redefine doPushTalkerInPool.
	Add a new method to Manager insertIpcTalker which will call icp::Service::insertTalker.
	The rationale of this recomendation is that one can only insert one talker to the
	ipc::Service, and this SHOULD be done at the start of the application.
*/

class IpcService: public fdt::ipc::Service{
	typedef fdt::ipc::Service 					BaseT;
	typedef DynamicReceiver<void, IpcService>	DynamicReceiverT;
public:	
	IpcService(uint32 _keepalivetout):BaseT(_keepalivetout){}
	~IpcService();
	
	static void dynamicRegister(){
		DynamicReceiverT::add<AddrInfoSignal, IpcService>();
	}
		
	int execute(ulong _sig, TimeSpec &_rtout);
	
	/*virtual*/ int signal(DynamicPointer<foundation::Signal> &_sig){
		if(this->state() < 0){
			_sig.clear();
			return 0;//no reason to raise the pool thread!!
		}
		dr.push(DynamicPointer<>(_sig));
		return Object::signal(fdt::S_SIG | fdt::S_RAISE);
	}
	
	int insertTalker(
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	void dynamicExecuteDefault(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<AddrInfoSignal> &_rsig);
protected:
	/*virtual*/void doPushTalkerInPool(foundation::aio::Object *_ptkr);
private:
	DynamicReceiverT		dr;
};

static const DynamicRegisterer<IpcService>	dr;

IpcService::~IpcService(){
	//Manager::the().removeService(this);
}

int IpcService::execute(ulong _sig, TimeSpec &_rtout){
	idbg("serviceexec");
	if(signaled()){
		ulong sm;
		{
			Mutex::Locker	lock(*mutex());
			sm = grabSignalMask(1);
			if(sm & fdt::S_SIG){//we have signals
				dr.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(dr.hasCurrent()){
				dr.executeCurrent(*this);
				dr.next();
			}
		}
		if(sm & fdt::S_KILL){
			idbgx(Dbg::ipc, "killing service "<<this->id());
			this->stop(true);
			Manager::the().removeService(this);
			return BAD;
		}
	}
	return NOK;
}

void IpcService::dynamicExecuteDefault(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}

void IpcService::dynamicExecute(DynamicPointer<AddrInfoSignal> &_rsig){
	int rv = this->insertTalker(_rsig->addrinfo.begin(), _rsig->node.c_str(), _rsig->service.c_str());
	_rsig->result(rv);
}
int IpcService::insertTalker(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	return fdt::ipc::Service::insertTalker(_rai, _node, _svc);
}

//=========================================================================

/*struct ExtraObjectPointer: fdt::ObjectPointer<fdt::Object>{
	ExtraObjectPointer(fdt::Object *_po);
	~ExtraObjectPointer();
	ExtraObjectPointer& operator=(fdt::Object *_pobj);
};

ExtraObjectPointer::ExtraObjectPointer(fdt::Object *_po):fdt::ObjectPointer<fdt::Object>(_po){}

ExtraObjectPointer::~ExtraObjectPointer(){
	this->destroy(this->release());
}
ExtraObjectPointer& ExtraObjectPointer::operator=(fdt::Object *_pobj){
	ptr(_pobj);
	return *this;
}*/
//=========================================================================
/*
	A local signal executer who knows how to unregister itself from the 
	manager
*/
class SignalExecuter: public fdt::SignalExecuter{
public:
	void removeFromManager();
};
void SignalExecuter::removeFromManager(){
	Manager::the().removeObject(this);
}

//=========================================================================
//The manager's localdata
struct Manager::Data{
//	typedef std::vector<ExtraObjectPointer>			ExtraObjectVector;
	typedef std::map<const char*, int, StrLess> 	ServiceIdxMap;
	typedef foundation::SelectPool<
		fdt::ObjectSelector
		>											ObjSelPoolT;
	typedef foundation::SelectPool<
		fdt::aio::Selector
		>											AioSelectorPoolT;

	Data(Manager &_rm);
	~Data();
//	ExtraObjectVector							eovec;
	ServiceIdxMap								servicemap;// map name -> service index
	//fdt::ipc::Service							*pcs; // A pointer to the ipc service
	ObjSelPoolT									*pobjectpool[2];//object pools
	AioSelectorPoolT							*paiopool[2];
	fdt::ObjectPointer<fdt::SignalExecuter>		readcmdexec;// read signal executer
	fdt::ObjectPointer<fdt::SignalExecuter>		writecmdexec;// write signal executer
	fdt::ObjectPointer<fdt::file::Manager>		pfm;
	IpcService									*pipc;
	//AioSelectorPoolT 							aiselpool1;
	//AioSelectorPoolT 							aiselpool2;
};

//--------------------------------------------------------------------------
typedef serialization::TypeMapper					TypeMapper;
typedef serialization::IdTypeMap					IdTypeMap;
typedef serialization::bin::Serializer				BinSerializer;


Manager::Data::Data(Manager &_rm)/*:aiselpool1(_rm, 10, 2048), aiselpool2(_rm, 10, 2048)*/
{
	pobjectpool[0] = NULL;
	pobjectpool[1] = NULL;
	
	paiopool[0] = NULL;
	paiopool[1] = NULL;
	
	TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);
	TypeMapper::registerSerializer<BinSerializer>();
	idbg("");
	if(true){
		pobjectpool[0] = new ObjSelPoolT(	_rm, 
											10,			//max thread cnt
											1024 * 4	//max objects per selector
											);			//at most 10 * 4 * 1024 connections
		pobjectpool[0]->start(1);//start with one worker
// 		pobjectpool[1] = new ObjSelPoolT(	_rm, 
// 											10,			//max thread cnt
// 											1024 * 4	//max objects per selector
// 											);			//at most 10 * 4 * 1024 connections
// 		pobjectpool[1]->start(1);//start with one worker
	}
	idbg("");
	if(true){
		paiopool[0] = new AioSelectorPoolT(_rm,
											10,			//max thread cnt
											2048		//max aio objects per selector/thread
											);			//at most 10 * 4 * 1024 connections
		paiopool[0]->start(1);//start with one worker
		
		paiopool[1] = new AioSelectorPoolT(_rm,
											10,			//max thread cnt
											2048		//max aio objects per selector/thread
											);			//at most 10 * 4 * 1024 connections
		paiopool[1]->start(1);//start with one worker
	}
	idbg("");
}

Manager::Data::~Data(){
	if(pobjectpool[0]) pobjectpool[0]->stop();
	if(pobjectpool[1]) pobjectpool[1]->stop();
	delete pobjectpool[0];
	delete pobjectpool[1];
	if(paiopool[0])paiopool[0]->stop();
	if(paiopool[1])paiopool[1]->stop();
	delete paiopool[0];
	delete paiopool[1];
	delete pfm.release();
	//delete readcmdexec.release();
	//delete writecmdexec.release();
}

//----------------------------------------------------------------------------------
template <>
void Manager::pushJob(fdt::Object *_pj, int _pos){
	d.pobjectpool[_pos]->push(fdt::ObjectPointer<fdt::Object>(_pj));
}
template <>
void Manager::pushJob(fdt::aio::Object *_pj, int _i){
	d.paiopool[_i]->push(fdt::ObjectPointer<fdt::aio::Object>(_pj));
}

/*
NOTE:
	It should be safe to give reference to 'this' to Data constructor, because
	it will be used in the SelectPool(s) constructor to call registerActiveSet(*this),
	which is defined in the base class of the manager which should be initialized first
*/

Manager::Manager():d(*(new Data(*this))){
	//NOTE: Use the following line instead of ThisGuard if you only have one Manager per process, else use the ThisGuard for any function
	// that may be called from a thread that has access to other managers.
	//this->prepareThread();
	
	ThisGuard	tg(this);
	if(true){//create register the file manager
		d.pfm = new fdt::file::Manager(new FileManagerController());
		fdt::Manager::insertObject(d.pfm.ptr());
		this->pushJob((fdt::Object*)d.pfm.ptr());
	}
	if(true){// create register the read signal executer
		d.readcmdexec = new SignalExecuter;
		fdt::Manager::insertObject(d.readcmdexec.ptr());
		this->pushJob((fdt::Object*)d.readcmdexec.ptr());
	}
	if(true){// create register the write signal executer
		d.writecmdexec = new SignalExecuter;
		fdt::Manager::insertObject(d.writecmdexec.ptr());
		this->pushJob((fdt::Object*)d.writecmdexec.ptr());
	}
	if(true){// create register the ipc service
		d.pipc = new IpcService(1000);//one sec keepalive tout
		int pos = fdt::Manager::insertService(d.pipc);
		if(pos < 0){
			idbg("unable to register service: "<<"ipc");
		}else{
			idbg("service "<<"ipc"<<" registered on pos "<<pos);
			this->pushJob((fdt::Object*)d.pipc);
			d.servicemap["ipc"] = pos;
		}
	}
}

Manager::~Manager(){
	ThisGuard	tg(this);
	fdt::Manager::stop(true);//wait all services to stop
	delete &d;
}

int Manager::start(const char *_which){
	ThisGuard	tg(this);
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).start();
		}
	}else{
		for(Data::ServiceIdxMap::iterator it(d.servicemap.begin());it != d.servicemap.end(); ++it){
			service(it->second).start();
		}
	}
	return OK;
}
int Manager::stop(const char *_which){
	ThisGuard	tg(this);
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).stop();
		}
	}else{
		fdt::Manager::stop(false);
	}
	return OK;
}

void Manager::readSignalExecuterUid(ObjectUidT &_ruid){
	_ruid.first = d.readcmdexec->id();
	_ruid.second = uid(*d.readcmdexec);
}
void Manager::writeSignalExecuterUid(ObjectUidT &_ruid){
	_ruid.first = d.writecmdexec->id();
	_ruid.second = uid(*d.writecmdexec);
}

int Manager::insertService(const char* _nm, Service* _psrvc){
	ThisGuard	tg(this);
	if(_psrvc != NULL){
		int pos = fdt::Manager::insertService(_psrvc);
		if(pos < 0){
			idbg("unable to register service: "<<_nm);
		}else{
			idbg("service "<<_nm<<" registered on pos "<<pos);
			//this->get<ObjSelPoolVecT>()[0]->push(fdt::ObjectPointer<fdt::Object>(_psrvc));
			this->pushJob((fdt::Object*)_psrvc);
			d.servicemap[_nm] = pos;
			return OK;
		}
	}else{
		idbg("service NULL");
	}
	return BAD;
}
void Manager::removeService(Service *_psrvc){
	fdt::Manager::removeService(_psrvc);
}
void Manager::removeService(fdt::Service *_psrvc){
	fdt::Manager::removeService(_psrvc);
}


int Manager::signalService(const char *_nm, DynamicPointer<fdt::Signal> &_rsig){
	//first find the service index
	ThisGuard	tg(this);
	Data::ServiceIdxMap::const_iterator it(d.servicemap.find(_nm));
	if(it == d.servicemap.end()) return BAD;
	ObjectUidT objuid(it->second, 0);
	return this->signalObject(objuid.first, objuid.second, _rsig);
}

int Manager::visitService(const char* _nm, Visitor &_rov){
	ThisGuard	tg(this);
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		//concept::Service *ts = static_cast<concept::Service*>(this->service(it->second));
		//TODO:
		//ts->visit(_rov);
		return OK;
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}
int Manager::insertIpcTalker(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	ThisGuard	tg(this);
	return Manager::the().ipc().insertTalker(_rai, _node, _svc);
}
foundation::ipc::Service&	Manager::ipc(){
	return *d.pipc;
}
foundation::file::Manager&	Manager::fileManager(){
	return *d.pfm;
}
void Manager::removeFileManager(){
	removeObject(d.pfm.ptr());
}
//----------------------------------------------------------------------------------
void IpcService::doPushTalkerInPool(foundation::aio::Object *_ptkr){
	Manager::the().pushJob(_ptkr, 1);
}
//----------------------------------------------------------------------------------
}//namespace concept

