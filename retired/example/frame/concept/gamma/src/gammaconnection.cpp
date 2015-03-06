#include <cstdio>

#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/socketaddress.hpp"
#include "system/thread.hpp"

#include "utility/binaryseeker.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/requestuid.hpp"


#include "core/manager.hpp"
#include "core/messages.hpp"

#include "gamma/gammaservice.hpp"

#include "gammaconnection.hpp"
#include "gammacommand.hpp"
#include "gammafilters.hpp"
#include "gammamessages.hpp"

#include "audit/log.hpp"

using namespace solid;
using namespace std;
//static const char	*hellostr = "Welcome to gamma service!!!\r\n"; 
//static const char *sigstr = "Signaled!!!\r\n";

namespace concept{
namespace gamma{

void Logger::doInFlush(const char *_pb, unsigned _bl){
	if(Log::the().isSet(Log::any, Log::Input)){
		Log::the().record(Log::Input, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::the().done();
	}
}

void Logger::doOutFlush(const char *_pb, unsigned _bl){
	if(Log::the().isSet(Log::any, Log::Output)){
		Log::the().record(Log::Output, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::the().done();
	}
}

/*static*/ void Connection::initStatic(Manager &_rm){
	//doInitStaticMaster(_rm);
	doInitStaticSlave(_rm);
	dynamicRegister();
}

namespace{
#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const unsigned specificIdStub(){
	static const uint id(Thread::specificId());
	return id;
}
void once_stub(){
	specificIdStub();
}

static const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_stub, once);
	return specificIdStub();
}
#endif
}

enum RetValE{
	Success,
	Wait,
	Failure,
	Close
};

/*static*/ Connection::DynamicMapperT		Connection::dm;

/*static*/ void Connection::dynamicRegister(){
	dm.insert<SocketMoveMessage, Connection>();
}

Connection::Connection(const SocketDevice &_rsd):
	BaseT(_rsd),isslave(true),crtreqid(1)
{
	sdv.push_back(new SocketData(0));
	sdv.back()->r.buffer(protocol::text::HeapBuffer(1024));
	sdv.back()->w.buffer(protocol::text::HeapBuffer(1024));
	socketState(0, SocketInit);
	this->socketPostEvents(0, frame::EventReschedule);
}


/*static*/ Connection& Connection::the(){
	return static_cast<Connection&>(frame::Object::specific());
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
	for(SocketDataVectorT::const_iterator it(sdv.begin()); it != sdv.end(); ++it){
		delete *it;
	}
}

/*virtual*/ bool Connection::notify(DynamicPointer<frame::Message> &_msgptr){
	if(this->state() < 0){
		_msgptr.clear();
		return false;//no reason to raise the pool thread!!
	}
	DynamicPointer<>	dp(_msgptr);
	dv.push_back(dp);
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}

/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	//_tout.add(2400);
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError)){
		idbg("timeout occured - destroy connection "<<state());
		_rexectx.close();
		return;
	}
	
	Manager &rm = Manager::the();
	frame::requestuidptr->set(this->id(), rm.id(*this).second);
	
	if(notified()){//we've received a signal
		DynamicHandler<DynamicMapperT>	dh(dm);
		ulong 							sm(0);
		{
			Locker<Mutex>	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL){
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(dv.begin(), dv.end());
				dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
			}
		}
		//now we determine if we return with Wait or we continue
		if(!_rexectx.eventMask()) return;
	}
	
	uint sigsz = this->signaledSize();
	while(sigsz--){
		uint sid = this->signaledFront();
		int rv = executeSocket(sid, _rexectx.currentTime());
		this->signaledPop();
		switch(rv){
			case Failure:
				delete sdv[sid]->pcmd;
				sdv[sid]->pcmd = NULL;
				delete sdv[sid];
				sdv[sid] = NULL;
				this->socketErase(sid);
				if(!this->count()){
					_rexectx.close();
					return;
				}
				break;
			case Success:
				this->socketPostEvents(sid, frame::EventReschedule);
				break;
			case Wait:
				break;
			case Close:
				_rexectx.close();
				return;
		}
	}
	if(this->signaledSize()){
		_rexectx.reschedule();
	}
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
		BinarySeekerResultT rv = reqbs(ridv.begin(), ridv.end(), rq);
		cassert(!rv.first);
		ridv.insert(ridv.begin() + rv.second, std::pair<uint32, int>(rq, _pos));
	}
	return rq;
}
bool   Connection::isRequestIdExpected(uint32 _v, int &_rpos)const{
	BinarySeekerResultT rv = reqbs(ridv.begin(), ridv.end(), _v);
	if(rv.first){
		_rpos = ridv[rv.second].second;
		return true;
	}else return false;
}
void   Connection::deleteRequestId(uint32 _v){
	if(_v == 0) return;
	BinarySeekerResultT rv = reqbs(ridv.begin(), ridv.end(), _v);
	cassert(!rv.first);
	ridv.erase(ridv.begin() + rv.second);
}
	
int Connection::executeSocket(const uint _sid, const TimeSpec &_tout){
	ulong		evs(this->socketEvents(_sid));
	
	if(evs & (frame::EventTimeoutRecv | frame::EventTimeoutSend)) return Failure;
	
	SocketData	&rsd(socketData(_sid));
	
	switch(socketState(_sid)){
		case SocketRegister:
			this->socketRequestRegister(_sid);
			socketState(_sid, SocketExecutePrepare);
			return Wait;
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
			if(evs & (frame::EventDoneIO)){
				socketState(_sid, SocketParse);
				return Success;
			}
			return Wait;//still waiting
		case SocketIdleParse:
			return doSocketParse(_sid, rsd, true);
		case SocketIdleWait:
			if(evs & (frame::EventDoneRecv)){
				socketState(_sid, SocketIdleParse);
				return Success;
			}
			if(!rsd.w.empty()){
				return doSocketExecute(_sid, rsd, SocketIdleWait);
			}
			return Wait;
		case SocketIdleDone:
			return doSocketExecute(_sid, rsd, SocketIdleDone);
		case SocketLeave:
			//the socket is unregistered
			//prepare a signal
			doSendSocketMessage(_sid);
			return Close;
		default:
			edbg("unknown state "<<socketState(_sid));
			cassert(false);
			
	}
	return Wait;
}

Command* Connection::create(const String& _name, Reader &_rr){
	//return isslave ? doCreateSlave(_name) : doCreateMaster(_name);
	return doCreateSlave(_name,_rr);
}

int Connection::doSocketPrepareBanner(const uint _sid, SocketData &_rsd){
	concept::Manager	&rm = concept::Manager::the();
	uint32				myport(rm.ipc().basePort());
	IndexT				objid(this->id());
	uint32				objuid(rm.id(*this).second);
	string				hoststr;
	string				portstr;
	SocketAddress		addr;
	
	_rsd.w<<"* Hello from gamma server ("<<myport<<" "<<(uint32)objid<<" "<<objuid<<") [";
	socketLocalAddress(_sid, addr);
	synchronous_resolve(
		hoststr,
		portstr,
		addr,
		ReverseResolveInfo::Numeric
	);
	_rsd.w<<hoststr<<':'<<portstr<<" -> ";
	socketRemoteAddress(_sid, addr);
	synchronous_resolve(
		hoststr,
		portstr,
		addr,
		ReverseResolveInfo::Numeric
	);
	_rsd.w<<hoststr<<':'<<portstr<<"]"<<'\r'<<'\n';
	_rsd.w.push(&Writer::flushAll);
	socketState(_sid, SocketExecute);
	return Success;
}
//prepare the reader and the writer for a new command
void Connection::doSocketPrepareParse(const uint _sid, SocketData &_rsd){
	_rsd.w.clear();
	_rsd.r.clear();
	//logger.outFlush();
	_rsd.r.push(&Reader::checkChar, protocol::text::Parameter('\n'));
	_rsd.r.push(&Reader::checkChar, protocol::text::Parameter('\r'));
	_rsd.r.push(&Reader::fetchKey<Reader, Connection, AtomFilter, Command>, protocol::text::Parameter(this, 64));
	_rsd.r.push(&Reader::checkChar, protocol::text::Parameter(' '));
	_rsd.r.push(&Reader::fetchFilteredString<TagFilter>, protocol::text::Parameter(&_rsd.w.tag(), 64));
	socketState(_sid, SocketParse);
}

int Connection::doSocketParse(const uint _sid, SocketData &_rsd, const bool _isidle){
	int rc(_rsd.r.run());
	switch(rc){
		case Reader::Success:
			//done parsing
			break;
		case Reader::Wait:
			if(socketHasPendingRecv(_sid)){
				socketTimeoutRecv(_sid, 300);
			}
			if(socketHasPendingSend(_sid)){
				socketTimeoutSend(_sid, 30);
			}
			if(_isidle){
				socketState(_sid, SocketIdleWait);
				return Success;
			}else{
				socketState(_sid, SocketParseWait);
				return Wait;
			}
		case Reader::Failure:
			edbg("");
			return Failure;
		case Reader::Yield:
			return Success;
		case Reader::Idle:
			cassert(!_isidle);
			socketState(_sid, SocketIdleParse);
			return Success;
	}
	if(_rsd.r.isError()){
		delete _rsd.pcmd; _rsd.pcmd = NULL;
		_rsd.w.push(Writer::putStatus);
		socketState(_sid, SocketExecute);
		return Success;
	}
	if(_isidle && !_rsd.w.empty())
		socketState(_sid, SocketIdleDone);
	else
		socketState(_sid, SocketExecutePrepare);
	return Success;
}

int Connection::doSocketExecute(const uint _sid, SocketData &_rsd, const int _state){
	if(socketHasPendingSend(_sid)) return solid::AsyncWait;
	int rc(_rsd.w.run());
	switch(rc){
		case Writer::Wait:
			socketTimeoutSend(_sid, 3000);
			//remain in the same state
			return Wait;
		case Writer::Success:
			if(!_state){
				delete _rsd.pcmd; _rsd.pcmd = NULL;
				socketState(_sid, SocketParsePrepare);
			}else if(_state == SocketIdleDone){
				socketState(_sid, SocketExecutePrepare);
			}
			//remain in the state and wait
			return Success;
		case Writer::Yield:
			return Success;
		case Writer::Leave:
			socketState(_sid, SocketLeave);
			this->socketRequestUnregister(_sid);
			return Wait;
		case Writer::Failure:
			edbg("");
			return Failure;
		default:
			edbg("rc = "<<rc);
			cassert(false);
			return Failure;
	}
	return Success;
}


void Connection::dynamicHandle(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}

void Connection::dynamicHandle(DynamicPointer<SocketMoveMessage> &_rmsgptr){
	vdbg("");
	//insert the new socket
	uint sid = this->socketInsert(_rmsgptr->sp);
	socketState(sid, SocketRegister);
	this->socketPostEvents(sid, frame::EventReschedule);
	if(sdv.size() > sid){
		sdv[sid] = _rmsgptr->psd;
	}else{
		sdv.push_back(_rmsgptr->psd);
		sdv.back()->w.buffer(protocol::text::HeapBuffer(1024));
		sdv.back()->r.buffer(protocol::text::HeapBuffer(1024));
	}
	_rmsgptr->psd = NULL;
	sdv[sid]->r.socketId(sid);
	sdv[sid]->w.socketId(sid);
	
}


void Connection::doSendSocketMessage(const uint _sid){
	ObjectUidT objuid;
	SocketData &rsd(socketData(_sid));
	rsd.pcmd->contextData(objuid);
	vdbg("Context data ("<<objuid.first<<','<<objuid.second<<')');
	concept::Manager	&rm = concept::Manager::the();
	frame::aio::SocketPointer sp;
	this->socketGrab(_sid, sp);
	DynamicPointer<frame::Message>	msgptr(new SocketMoveMessage(sp, sdv[_sid]));
	sdv[_sid] = NULL;
	rm.notify(msgptr, ObjectUidT(objuid.first, objuid.second));
}

void Connection::appendContextString(std::string &_str){
	concept::Manager	&rm = concept::Manager::the();
	char				buffer[256];
	IndexT				objid(this->id());
	uint32				objuid(rm.id(*this).second);
	
	
	if(sizeof(objid) == 8){
		sprintf(buffer, "%llX-%X", (unsigned long long)objid, objuid);
	}else{
		sprintf(buffer, "%lX-%X", objid, objuid);
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

SocketMoveMessage::~SocketMoveMessage(){
	if(psd){
		delete psd->pcmd;
		delete psd;
	}
}

}//namespace gamma
}//namespace concept
