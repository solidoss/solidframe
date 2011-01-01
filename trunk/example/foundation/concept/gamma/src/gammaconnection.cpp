#include <cstdio>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/socketaddress.hpp"
#include "system/thread.hpp"

#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "utility/iostream.hpp"
#include "utility/binaryseeker.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/requestuid.hpp"


#include "core/manager.hpp"
#include "core/signals.hpp"

#include "gamma/gammaservice.hpp"

#include "gammaconnection.hpp"
#include "gammacommand.hpp"
#include "gammafilters.hpp"
#include "gammasignals.hpp"

#include "audit/log.hpp"

namespace fdt=foundation;
//static const char	*hellostr = "Welcome to gamma service!!!\r\n"; 
//static const char *sigstr = "Signaled!!!\r\n";

namespace concept{
namespace gamma{

void Logger::doInFlush(const char *_pb, unsigned _bl){
	if(Log::instance().isSet(Log::any, Log::Input)){
		Log::instance().record(Log::Input, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::instance().done();
	}
}

void Logger::doOutFlush(const char *_pb, unsigned _bl){
	if(Log::instance().isSet(Log::any, Log::Output)){
		Log::instance().record(Log::Output, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::instance().done();
	}
}

/*static*/ void Connection::initStatic(Manager &_rm){
	//doInitStaticMaster(_rm);
	doInitStaticSlave(_rm);
}

namespace{
static const DynamicRegisterer<Connection>	dre;
static const unsigned specificPosition(){
	//TODO: staticproblem
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
}

/*static*/ void Connection::dynamicRegister(){
	DynamicExecuterT::registerDynamic<IStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<OStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<IOStreamSignal, Connection>();
	DynamicExecuterT::registerDynamic<StreamErrorSignal, Connection>();
	DynamicExecuterT::registerDynamic<SocketMoveSignal, Connection>();
}

Connection::Connection(const SocketDevice &_rsd):
	BaseT(_rsd),isslave(true),crtreqid(1)
{
	sdv.push_back(new SocketData(0));
	sdv.back()->r.buffer(protocol::HeapBuffer(1024));
	sdv.back()->w.buffer(protocol::HeapBuffer(1024));
	socketState(0, SocketInit);
	this->socketPostEvents(0, fdt::RESCHEDULED);
}


/*static*/ Connection& Connection::the(){
	return *reinterpret_cast<Connection*>(Thread::specific(specificPosition()));
}

/*
NOTE: 
* Releasing the connection here and not in a base class destructor because
here we know the exact type of the object - so the service can do other things 
based on the type.
* Also it ensures a proper/safe visiting. Suppose the unregister would have taken 
place in a base destructor. If the visited object is a leaf, one may visit
destroyed data.
NOTE: 
* Visitable data must be destroyed after releasing the connection!!!
*/

Connection::~Connection(){
	idbg("destroy connection id "<<this->id());
	Thread::specific(specificPosition(), this);
	for(SocketDataVectorT::const_iterator it(sdv.begin()); it != sdv.end(); ++it){
		delete *it;
	}
}

/*virtual*/ bool Connection::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	dr.push(DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}

/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

int Connection::execute(ulong _sig, TimeSpec &_tout){
	//_tout.add(2400);
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
		idbg("timeout occured - destroy connection "<<state());
		return BAD;
	}
	
	Manager &rm = Manager::the();
	fdt::requestuidptr->set(this->id(), rm.uid(*this));
	Thread::specific(specificPosition(), this);
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
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
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	
	uint sigsz = this->signaledSize();
	while(sigsz--){
		uint sid = this->signaledFront();
		int rv = executeSocket(sid, _tout);
		this->signaledPop();
		switch(rv){
			case BAD:
				delete sdv[sid]->pcmd;
				sdv[sid]->pcmd = NULL;
				delete sdv[sid];
				sdv[sid] = NULL;
				this->socketErase(sid);
				if(!this->count()) return BAD;
				break;
			case OK:
				this->socketPostEvents(sid, fdt::RESCHEDULED);
				break;
			case NOK:
				break;
			case fdt::LEAVE:
				return BAD;
		}
	}
	if(this->signaledSize()) return OK;
	return NOK;
}


struct ReqCmp{
	int operator()(const std::pair<uint32, int> &_v1, const uint32 _v2)const{
		//TODO: _v1.first is an id - problems when is near ffffffff
		//see ipc::processconnector.cpp
		if(_v1.first < _v2) return -1;
		if(_v1.first > _v2) return 1;
		return 0;
	}
};

static BinarySeeker<ReqCmp>	reqbs;

uint32 Connection::newRequestId(int _pos){
	uint32 rq = crtreqid;
	++crtreqid;
	if(crtreqid == 0) crtreqid = 1;
	if(rq > ridv.back().first){
		//push back
		ridv.push_back(std::pair<uint32, int>(rq, _pos));
		
	}else{
		int rv = reqbs(ridv.begin(), ridv.end(), rq);
		cassert(rv < 0);
		rv = -rv - 1;
		ridv.insert(ridv.begin() + rv, std::pair<uint32, int>(rq, _pos));
	}
	return rq;
}
bool   Connection::isRequestIdExpected(uint32 _v, int &_rpos)const{
	int rv = reqbs(ridv.begin(), ridv.end(), _v);
	if(rv >= 0){
		_rpos = ridv[rv].second;
		return true;
	}else return false;
}
void   Connection::deleteRequestId(uint32 _v){
	if(_v == 0) return;
	int rv = reqbs(ridv.begin(), ridv.end(), _v);
	cassert(rv < 0);
	rv = -rv - 1;
	ridv.erase(ridv.begin() + rv);
}
	
int Connection::executeSocket(const uint _sid, const TimeSpec &_tout){
	ulong evs(this->socketEvents(_sid));
	if(evs & (fdt::TIMEOUT_SEND | fdt::TIMEOUT_RECV)) return BAD;
	SocketData &rsd(socketData(_sid));
	switch(socketState(_sid)){
		case SocketRegister:
			this->socketRequestRegister(_sid);
			socketState(_sid, SocketExecutePrepare);
			return NOK;
		case SocketInit:
			//TODO:
			socketState(_sid, SocketBanner);
		case SocketBanner:
			return doSocketPrepareBanner(_sid, rsd);
		case SocketParsePrepare:
			doSocketPrepareParse(_sid, rsd);
		case SocketParse:
			return doSocketParse(_sid, rsd);
		case SocketExecutePrepare:
			rsd.pcmd->execute(_sid);
			socketState(_sid, SocketExecute);
		case SocketExecute:
			return doSocketExecute(_sid, rsd);
		case SocketParseWait:
			if(evs & (fdt::INDONE | fdt::DONE |fdt::OUTDONE)){
				socketState(_sid, SocketParse);
				return OK;
			}
			return NOK;//still waiting
		case SocketIdleParse:
			return doSocketParse(_sid, rsd, true);
		case SocketIdleWait:
			if(evs & (fdt::INDONE)){
				socketState(_sid, SocketIdleParse);
				return OK;
			}
			if(!rsd.w.empty()){
				return doSocketExecute(_sid, rsd, SocketIdleWait);
			}
			return NOK;
		case SocketIdleDone:
			return doSocketExecute(_sid, rsd, SocketIdleDone);
		case SocketLeave:
			cassert(evs & fdt::DONE);
			//the socket is unregistered
			//prepare a signal
			doSendSocketSignal(_sid);
			return fdt::LEAVE;
		default:
			edbg("unknown state "<<socketState(_sid));
			cassert(false);
			
	}
	return NOK;
}

Command* Connection::create(const String& _name, Reader &_rr){
	//return isslave ? doCreateSlave(_name) : doCreateMaster(_name);
	return doCreateSlave(_name,_rr);
}

int Connection::doSocketPrepareBanner(const uint _sid, SocketData &_rsd){
	concept::Manager	&rm = concept::Manager::the();
	uint32				myport(rm.ipc().basePort());
	ulong				objid(this->id());
	uint32				objuid(rm.uid(*this));
	char				host[SocketAddress::MaxSockHostSz];
	char				port[SocketAddress::MaxSockServSz];
	SocketAddress		addr;
	
	_rsd.w<<"* Hello from gamma server ("<<myport<<" "<<(uint32)objid<<" "<<objuid<<") [";
	socketLocalAddress(_sid, addr);
	addr.name(
		host,
		SocketAddress::MaxSockHostSz,
		port,
		SocketAddress::MaxSockServSz,
		SocketAddress::NumericService | SocketAddress::NumericHost
	);
	_rsd.w<<host<<':'<<port<<" -> ";
	socketRemoteAddress(_sid, addr);
	addr.name(
		host,
		SocketAddress::MaxSockHostSz,
		port,
		SocketAddress::MaxSockServSz,
		SocketAddress::NumericService | SocketAddress::NumericHost
	);
	_rsd.w<<host<<':'<<port<<"]"<<'\r'<<'\n';
	_rsd.w.push(&Writer::flushAll);
	socketState(_sid, SocketExecute);
	return OK;
}
//prepare the reader and the writer for a new command
void Connection::doSocketPrepareParse(const uint _sid, SocketData &_rsd){
	_rsd.w.clear();
	_rsd.r.clear();
	//logger.outFlush();
	_rsd.r.push(&Reader::checkChar, protocol::Parameter('\n'));
	_rsd.r.push(&Reader::checkChar, protocol::Parameter('\r'));
	_rsd.r.push(&Reader::fetchKey<Reader, Connection, AtomFilter, Command>, protocol::Parameter(this, 64));
	_rsd.r.push(&Reader::checkChar, protocol::Parameter(' '));
	_rsd.r.push(&Reader::fetchFilteredString<TagFilter>, protocol::Parameter(&_rsd.w.tag(), 64));
	socketState(_sid, SocketParse);
}

int Connection::doSocketParse(const uint _sid, SocketData &_rsd, const bool _isidle){
	int rc(_rsd.r.run());
	switch(rc){
		case OK:
			//done parsing
			break;
		case NOK:
			if(socketHasPendingRecv(_sid)){
				socketTimeoutRecv(_sid, 300);
			}
			if(socketHasPendingSend(_sid)){
				socketTimeoutSend(_sid, 30);
			}
			if(_isidle){
				socketState(_sid, SocketIdleWait);
				return OK;
			}else{
				socketState(_sid, SocketParseWait);
				return NOK;
			}
		case BAD:
			edbg("");
			return BAD;
		case YIELD:
			return OK;
		case Reader::Idle:
			cassert(!_isidle);
			socketState(_sid, SocketIdleParse);
			return OK;
	}
	if(_rsd.r.isError()){
		delete _rsd.pcmd; _rsd.pcmd = NULL;
		_rsd.w.push(Writer::putStatus);
		socketState(_sid, SocketExecute);
		return OK;
	}
	if(_isidle && !_rsd.w.empty())
		socketState(_sid, SocketIdleDone);
	else
		socketState(_sid, SocketExecutePrepare);
	return OK;
}

int Connection::doSocketExecute(const uint _sid, SocketData &_rsd, const int _state){
	if(socketHasPendingSend(_sid)) return NOK;
	int rc(_rsd.w.run());
	switch(rc){
		case NOK:
			socketTimeoutSend(_sid, 3000);
			//remain in the same state
			return NOK;
		case OK:
			if(!_state){
				delete _rsd.pcmd; _rsd.pcmd = NULL;
				socketState(_sid, SocketParsePrepare);
				return OK;
			}else if(_state == SocketIdleDone){
				socketState(_sid, SocketExecutePrepare);
				return OK;
			}
			//remain in the state and wait
			return OK;
		case YIELD:
			return OK;
		case Writer::Leave:
			socketState(_sid, SocketLeave);
			this->socketRequestUnregister(_sid);
			return NOK;
		default:
			edbg("rc = "<<rc);
			return rc;
	}
	return OK;
}


void Connection::dynamicExecute(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}


void Connection::dynamicExecute(DynamicPointer<IStreamSignal> &_psig){
	int sid(-1);
	if(!isRequestIdExpected(_psig->requid.first, sid)) return;
	cassert(sid >= 0);
	SocketData &rsd(socketData(sid));
	int rv = rsd.pcmd->receiveIStream(_psig->sptr, _psig->fileuid, 0, ObjectUidT(), NULL);
	cassert(rv != OK);
	this->socketPostEvents(sid, fdt::RESCHEDULED);
}

void Connection::dynamicExecute(DynamicPointer<OStreamSignal> &_psig){
}

void Connection::dynamicExecute(DynamicPointer<IOStreamSignal> &_psig){
}

void Connection::dynamicExecute(DynamicPointer<StreamErrorSignal> &_psig){
}

void Connection::dynamicExecute(DynamicPointer<SocketMoveSignal> &_psig){
	vdbg("");
	//insert the new socket
	uint sid = this->socketInsert(_psig->sp);
	socketState(sid, SocketRegister);
	this->socketPostEvents(sid, fdt::RESCHEDULED);
	if(sdv.size() > sid){
		sdv[sid] = _psig->psd;
	}else{
		sdv.push_back(_psig->psd);
		sdv.back()->w.buffer(protocol::HeapBuffer(1024));
		sdv.back()->r.buffer(protocol::HeapBuffer(1024));
	}
	_psig->psd = NULL;
	sdv[sid]->r.socketId(sid);
	sdv[sid]->w.socketId(sid);
	
}


void Connection::doSendSocketSignal(const uint _sid){
	ObjectUidT objuid;
	SocketData &rsd(socketData(_sid));
	rsd.pcmd->contextData(objuid);
	vdbg("Context data ("<<objuid.first<<','<<objuid.second<<')');
	concept::Manager	&rm = concept::Manager::the();
	fdt::aio::SocketPointer sp;
	this->socketGrab(_sid, sp);
	DynamicPointer<fdt::Signal>	psig(new SocketMoveSignal(sp, sdv[_sid]));
	sdv[_sid] = NULL;
	rm.signal(psig, objuid.first, objuid.second);
}

void Connection::appendContextString(std::string &_str){
	concept::Manager	&rm = concept::Manager::the();
	char				buffer[256];
	foundation::IndexT	objid(this->id());
	uint32				objuid(rm.uid(*this));
	
	
	if(sizeof(objid) == 8){
		sprintf(buffer, "%llX-%X", objid, objuid);
	}else{
		sprintf(buffer, "%X-%X", objid, objuid);
	}
	_str += buffer;
}

//---------------------------------------------------------------
// Command Base
//---------------------------------------------------------------
//typedef serialization::TypeMapper					TypeMapper;
//typedef serialization::bin::Serializer				BinSerializer;
//typedef serialization::bin::Deserializer			BinDeserializer;

Command::Command(){}
void Command::initStatic(Manager &_rm){
//	TypeMapper::map<SendStringSignal, BinSerializer, BinDeserializer>();
}
/*virtual*/ Command::~Command(){}
void Command::contextData(ObjectUidT &_robjuid){}
int Command::receiveIStream(
	StreamPointer<IStream> &_ps,
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveOStream(
	StreamPointer<OStream> &,
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveIOStream(
	StreamPointer<IOStream> &, 
	const FileUidT &,
	int			_which,
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveString(
	const String &_str,
	int			_which, 
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int receiveData(
	void *_pdata,
	int _datasz,
	int			_which, 
	const ObjectUidT&_from,
	const foundation::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveNumber(
	const int64 &_no,
	int			_which,
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveData(
	void *_v,
	int	_vsz,
	int			_which,
	const ObjectUidT&_from,
	const fdt::ipc::ConnectionUid *_conid
){
	return BAD;
}
int Command::receiveError(
	int _errid,
	const ObjectUidT&_from,
	const foundation::ipc::ConnectionUid *_conid
){
	return BAD;
}

SocketMoveSignal::~SocketMoveSignal(){
	if(psd){
		delete psd->pcmd;
		delete psd;
	}
}

}//namespace gamma
}//namespace concept
