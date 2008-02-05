/* Implementation file server.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <map>
#include <vector>

#include "system/debug.h"

#include "utility/iostream.h"

#include "algorithm/serialization/binary.h"

#include "core/server.h"
#include "core/service.h"
#include "core/visitor.h"
#include "core/command.h"
#include "core/connection.h"
#include "core/object.h"


#include "clientserver/core/selectpool.h"
#include "clientserver/core/execpool.h"
#include "clientserver/core/filemanager.h"
#include "clientserver/core/filekeys.h"
#include "clientserver/tcp/connectionselector.h"
#include "clientserver/tcp/connection.h"
#include "clientserver/tcp/listenerselector.h"
#include "clientserver/tcp/listener.h"
#include "clientserver/udp/talkerselector.h"
#include "clientserver/udp/talker.h"
#include "clientserver/core/objectselector.h"
#include "clientserver/core/commandexecuter.h"

#include "clientserver/ipc/ipcservice.h"


#include <iostream>
using namespace std;

//some forward declarations
namespace clientserver{

class ObjectSelector;

namespace tcp{
class ConnectionSelector;
class ListenerSelector;
}//namespace tcp

namespace udp{
class TalkerSelector;
}//namespace udp

}//namespace clientserver

namespace cs=clientserver;


namespace test{

//======= FileManager:==================================================
//typedef cs::FileUidTp			FileUidTp;
typedef Object::RequestUidTp	RequestUidTp;

struct IStreamCommand: test::Command{
	IStreamCommand(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(Connection &_pcon);
	int execute(Object &_pobj);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &);
	StreamPtr<IStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};

int IStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveIStream(sptr, fileuid, requid);
}

int IStreamCommand::execute(Object &_robj){
	return _robj.receiveIStream(sptr, fileuid, requid);
}

int IStreamCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &, TimeSpec &){
	_rce.receiveIStream(sptr, fileuid, requid);
	return BAD;
}

struct OStreamCommand: test::Command{
	OStreamCommand(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(Connection &_pcon);
	int execute(Object &_pobj);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &);
	StreamPtr<OStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};

int OStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveOStream(sptr, fileuid, requid);
}

int OStreamCommand::execute(Object &_robj){
	return _robj.receiveOStream(sptr, fileuid, requid);
}
int OStreamCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &, TimeSpec &){
	_rce.receiveOStream(sptr, fileuid, requid);
	return BAD;
}

struct IOStreamCommand: test::Command{
	IOStreamCommand(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(Connection &_pcon);
	int execute(Object &_pobj);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &);
	StreamPtr<IOStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};

int IOStreamCommand::execute(Connection &_rcon){
	return _rcon.receiveIOStream(sptr, fileuid, requid);
}

int IOStreamCommand::execute(Object &_robj){
	return _robj.receiveIOStream(sptr, fileuid, requid);
}

int IOStreamCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &, TimeSpec &){
	_rce.receiveIOStream(sptr, fileuid, requid);
	return BAD;
}

struct StreamErrorCommand: test::Command{
	StreamErrorCommand(int _errid, const RequestUidTp &_requid):errid(_errid), requid(_requid){}
	int execute(Connection &_pcon);
	int execute(Object &_pobj);
	int execute(cs::CommandExecuter&, const CommandUidTp &, TimeSpec &);
	int				errid;
	RequestUidTp	requid;
};

int StreamErrorCommand::execute(Connection &_rcon){
	return _rcon.receiveError(errid, requid);
}
int StreamErrorCommand::execute(Object &_robj){
	return _robj.receiveError(errid, requid);
}
int StreamErrorCommand::execute(cs::CommandExecuter& _rce, const CommandUidTp &, TimeSpec &){
	_rce.receiveError(errid, requid);
	return BAD;
}

class FileManager: public cs::FileManager{
public:
	FileManager(uint32 _maxfcnt = 256):cs::FileManager(_maxfcnt){}
protected:
	/*virtual*/ void sendStream(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid);
	/*virtual*/ void sendStream(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid);
	/*virtual*/ void sendStream(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid);
	/*virtual*/ void sendError(int _errid, const RequestUid& _rrequid);
};
void FileManager::sendStream(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid){
	cs::CmdPtr<cs::Command>	cp(new IStreamCommand(_sptr, _rfuid, RequestUidTp(_rrequid.reqidx, _rrequid.requid)));
	Server::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}
void FileManager::sendStream(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid){
	cs::CmdPtr<cs::Command>	cp(new OStreamCommand(_sptr, _rfuid, RequestUidTp(_rrequid.reqidx, _rrequid.requid)));
	Server::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}
void FileManager::sendStream(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUid& _rrequid){
	cs::CmdPtr<cs::Command>	cp(new IOStreamCommand(_sptr, _rfuid, RequestUidTp(_rrequid.reqidx, _rrequid.requid)));
	Server::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}
void FileManager::sendError(int _error, const RequestUid& _rrequid){
	cs::CmdPtr<cs::Command>	cp(new StreamErrorCommand(_error, RequestUidTp(_rrequid.reqidx, _rrequid.requid)));
	Server::the().signalObject(_rrequid.objidx, _rrequid.objuid, cp);
}

//======= IpcService ======================================================
class IpcService: public cs::ipc::Service{
public:
	IpcService(serialization::bin::RTTIMapper& _rm):cs::ipc::Service(_rm){}
protected:
	/*virtual*/void pushTalkerInPool(clientserver::Server &_rs, clientserver::udp::Talker *_ptkr);
};

//=========================================================================

struct ExtraObjPtr: cs::ObjPtr<cs::Object>{
	ExtraObjPtr(cs::Object *_po);
	~ExtraObjPtr();
	ExtraObjPtr& operator=(cs::Object *_pobj);
};

ExtraObjPtr::ExtraObjPtr(cs::Object *_po):cs::ObjPtr<cs::Object>(_po){}

ExtraObjPtr::~ExtraObjPtr(){
	this->destroy(this->release());
}
ExtraObjPtr& ExtraObjPtr::operator=(cs::Object *_pobj){
	ptr(_pobj);
	return *this;
}
//=========================================================================
class CommandExecuter: public cs::CommandExecuter{
public:
	void removeFromServer();
};
void CommandExecuter::removeFromServer(){
	Server::the().removeObject(this);
}
// class WriteCommandExecuter: public cs::CommandExecuter{
// public:
// 	~WriteCommandExecuter(){
// 		Server::the().removeCommandExecuter(this);
// 	}
// };

//=========================================================================

struct Server::Data{
	typedef std::vector<ExtraObjPtr>								ExtraObjectVector;
	typedef std::map<const char*, int, StrLess> 					ServiceIdxMap;
	typedef clientserver::SelectPool<cs::ObjectSelector>			ObjSelPoolTp;
	typedef clientserver::SelectPool<cs::tcp::ConnectionSelector>	ConSelPoolTp;
	typedef clientserver::SelectPool<cs::tcp::ListenerSelector>		LisSelPoolTp;
	typedef clientserver::SelectPool<cs::udp::TalkerSelector>		TkrSelPoolTp;

	Data(Server &_rs);
	~Data();
	ExtraObjectVector					eovec;
	ServiceIdxMap						servicemap;
	cs::ipc::Service					*pcs;
	serialization::bin::RTTIMapper		binmapper;
	ObjSelPoolTp						*pobjectpool[2];
	ConSelPoolTp						*pconnectionpool;
	LisSelPoolTp						*plistenerpool;
	TkrSelPoolTp						*ptalkerpool;
	cs::ObjPtr<cs::CommandExecuter>		readcmdexec;
	cs::ObjPtr<cs::CommandExecuter>		writecmdexec;
};

//--------------------------------------------------------------------------

Server::Data::Data(Server &_rs):pconnectionpool(NULL), plistenerpool(NULL), ptalkerpool(NULL){
	pobjectpool[0] = NULL;
	pobjectpool[1] = NULL;
	if(true){	
		pconnectionpool = new ConSelPoolTp(	_rs,
												10,		//max thread cnt
												50		//max connections per selector
												);		//at most 10 * 4 * 1024 connections
		pconnectionpool->start(1);//start with one worker
	}
	if(true){
		plistenerpool = new LisSelPoolTp(	_rs, 
												2, 	//max thread cnt
												128	//max listeners per selector 
												);	//at most 128*2 = 256 listeners
		plistenerpool->start(1);//start with one worker
	}
	if(true){
		ptalkerpool = new TkrSelPoolTp(	_rs, 
												2, 	//max thread cnt
												128	//max listeners per selector 
												);	//at most 128*2 = 256 listeners
		ptalkerpool->start(1);//start with one worker
	}
	if(true){
		pobjectpool[0] = new ObjSelPoolTp(	_rs, 
												10,		//max thread cnt
												1024 * 4//max objects per selector
												);		//at most 10 * 4 * 1024 connections
		pobjectpool[0]->start(1);//start with one worker
	}
}

Server::Data::~Data(){
	if(pconnectionpool) pconnectionpool->stop();
	delete pconnectionpool;
	
	if(plistenerpool) plistenerpool->stop();
	delete plistenerpool;
	
	if(ptalkerpool) ptalkerpool->stop();
	delete ptalkerpool;
	
	if(pobjectpool[0]) pobjectpool[0]->stop();
	if(pobjectpool[1]) pobjectpool[1]->stop();
	delete pobjectpool[0];
	delete pobjectpool[1];
	delete readcmdexec.release();
	delete writecmdexec.release();
}

//----------------------------------------------------------------------------------
ServiceMap& getServiceMap(){
	static ServiceMap sm;
	return sm;
}

void registerService(ServiceCreator _psc, const char* _pname){
	idbg("registering "<<_pname);
	cout<<"registering "<<_pname<<endl;
	getServiceMap()[_pname] = _psc;
}
//----------------------------------------------------------------------------------
template <>
void Server::pushJob(cs::tcp::Listener *_pj, int){
	d.plistenerpool->push(cs::ObjPtr<cs::tcp::Listener>(_pj));
}
template <>
void Server::pushJob(cs::udp::Talker *_pj, int){
	d.ptalkerpool->push(cs::ObjPtr<cs::udp::Talker>(_pj));
}
template <>
void Server::pushJob(cs::tcp::Connection *_pj, int){
	d.pconnectionpool->push(cs::ObjPtr<cs::tcp::Connection>(_pj));
}
template <>
void Server::pushJob(cs::Object *_pj, int _pos){
	d.pobjectpool[_pos]->push(cs::ObjPtr<cs::Object>(_pj));
}

/*
NOTE:
	It should be safe to give reference to 'this' to Data constructor, because
	it will be used in the SelectPool(s) constructor to call registerActiveSet(*this),
	which is defined in the base class of the server which should be initialized first
*/

Server::Server():d(*(new Data(*this))){
	//ppools = new PoolContainer(*this);
	if(true){
		this->fileManager(new FileManager(50));
		cs::Server::insertObject(&fileManager());
		cs::NameFileKey::registerMapper(fileManager());
		cs::TempFileKey::registerMapper(fileManager());
		this->pushJob((cs::Object*)&fileManager());
	}
	if(true){
		d.readcmdexec = new CommandExecuter;
		cs::Server::insertObject(d.readcmdexec.ptr());
		this->pushJob((cs::Object*)d.readcmdexec.ptr());
	}
	if(true){
		d.writecmdexec = new CommandExecuter;
		cs::Server::insertObject(d.writecmdexec.ptr());
		this->pushJob((cs::Object*)d.writecmdexec.ptr());
	}
	if(true){
		this->ipc(new IpcService(d.binmapper));
		int pos = cs::Server::insertService(&this->ipc());
		if(pos < 0){
			idbg("unable to register service: "<<"ipc");
		}else{
			idbg("service "<<"ipc"<<" registered on pos "<<pos);
			this->pushJob((cs::Object*)&this->ipc());
			//do not map the ipc!!!
			//d.servicemap["ipc"] = pos;
		}
	}
}

Server::~Server(){
	//service(0).stop(*this, true);//Stop all services
	cs::Server::stop(true);
	delete &d;
}
/*void Server::removeCommandExecuter(ReadCommandExecuter *_pce){
	removeObject(_pce);
}
void Server::removeCommandExecuter(WriteCommandExecuter *_pce){
	removeObject(_pce);
}*/
serialization::bin::RTTIMapper &Server::binMapper(){
	return d.binmapper;
}

int Server::start(const char *_which){
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).start(*this);
		}
	}else{
		for(Data::ServiceIdxMap::iterator it(d.servicemap.begin());it != d.servicemap.end(); ++it){
			service(it->second).start(*this);
		}
	}
	return OK;
}
int Server::stop(const char *_which){
	if(_which){
		Data::ServiceIdxMap::iterator it(d.servicemap.find(_which));
		if(it != d.servicemap.end()){
			service(it->second).stop(*this);
		}
	}else{
// 		for(Data::ServiceIdxMap::iterator it(d.servicemap.begin());it != d.servicemap.end(); ++it){
// 			service(it->second).stop(*this, false);//do not wait for stopping
// 		}
		cs::Server::stop(false);
	}
	return OK;
}

void Server::readCommandExecuterUid(ObjectUidTp &_ruid){
	_ruid.first = d.readcmdexec->id();
	_ruid.second = uid(*d.readcmdexec);
}
void Server::writeCommandExecuterUid(ObjectUidTp &_ruid){
	_ruid.first = d.writecmdexec->id();
	_ruid.second = uid(*d.writecmdexec);
}

int Server::insertService(const char* _nm, Service* _psrvc){
	if(_psrvc != NULL){
		int pos = cs::Server::insertService(_psrvc);
		if(pos < 0){
			idbg("unable to register service: "<<_nm);
		}else{
			idbg("service "<<_nm<<" registered on pos "<<pos);
			//this->get<ObjSelPoolVecTp>()[0]->push(cs::ObjPtr<cs::Object>(_psrvc));
			this->pushJob((cs::Object*)_psrvc);
			d.servicemap[_nm] = pos;
			return OK;
		}
	}else{
		idbg("service NULL");
	}
	return BAD;
}
void Server::removeService(Service *_psrvc){
	cs::Server::removeService(_psrvc);
}

int Server::insertListener(const char* _nm, const AddrInfoIterator &_rai){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertListener(*this, _rai);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::insertTalker(const char* _nm, const AddrInfoIterator &_rai, const char*_node, const char *_srv){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertTalker(*this, _rai, _node, _srv);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::insertConnection(const char* _nm, const AddrInfoIterator &_rai, const char*_node, const char *_srv){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		test::Service &ts = static_cast<test::Service&>(this->service(it->second));
		return ts.insertConnection(*this, _rai, _node, _srv);
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}

int Server::visitService(const char* _nm, Visitor &_rov){
	Data::ServiceIdxMap::iterator it(d.servicemap.find(_nm));
	if(it != d.servicemap.end()){
		//test::Service *ts = static_cast<test::Service*>(this->service(it->second));
		//TODO:
		//ts->visit(_rov);
		return OK;
	}else{
		idbg("service not found "<<d.servicemap.size());
		return BAD;
	}
}
//----------------------------------------------------------------------------------
void IpcService::pushTalkerInPool(clientserver::Server &_rs, clientserver::udp::Talker *_ptkr){
	static_cast<Server&>(_rs).pushJob(_ptkr);
}
//----------------------------------------------------------------------------------

int Command::execute(Connection &){
	cassert(false);
	return BAD;
}

int Command::execute(Listener &){
	cassert(false);
	return BAD;
}

int Command::execute(Object &){
	cassert(false);
	return BAD;
}

//----------------------------------------------------------------------------------

int Connection::receiveIStream(
	StreamPtr<IStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp& _from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Connection::receiveOStream(
	StreamPtr<OStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp& _from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Connection::receiveIOStream(
	StreamPtr<IOStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Connection::receiveString(
	const String &_str,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Connection::receiveNumber(
	const int64 &_no,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Connection::receiveError(
	int _errid, 
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}
//----------------------------------------------------------------------------------

int Object::receiveIStream(
	StreamPtr<IStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Object::receiveOStream(
	StreamPtr<OStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Object::receiveIOStream(
	StreamPtr<IOStream> &,
	const FileUidTp	&,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

int Object::receiveString(
	const String &_str,
	const RequestUidTp &_requid,
	int			_which,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}
int Object::receiveError(
	int _errid, 
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	cassert(false);
	return BAD;
}

//----------------------------------------------------------------------------------
}//namespace test

