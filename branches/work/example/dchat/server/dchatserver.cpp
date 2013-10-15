#include "protocol/binary/binaryaiosession.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"
#include "protocol/binary/binaryspecificbuffercontroller.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcmessage.hpp"


#include <signal.h>

#include "boost/program_options.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

class Connection;

struct ConnectionContext{
	ConnectionContext(Connection &_rcon):rcon(_rcon), sndmsgidx(-1),rcvmsgidx(-1){}
	void sendMessageIndex(const uint32 _msgidx){
		sndmsgidx = _msgidx;
	}
	void recvMessageIndex(const uint32 _msgidx){
		rcvmsgidx = _msgidx;
	}
	Connection	&rcon;
	uint32		sndmsgidx;
	uint32		rcvmsgidx;
};

typedef serialization::binary::Serializer<ConnectionContext>	BinSerializerT;
typedef serialization::binary::Deserializer<ConnectionContext>	BinDeserializerT;
typedef serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, uint8,
	serialization::FakeMutex
>																UInt8TypeMapperT;

class IpcController;

struct BaseMessage: Dynamic<BaseMessage, DynamicShared<frame::ipc::Message> >{
private:
	friend class IpcController;
	virtual void ipcOnReceive(
		frame::ipc::ConnectionContext const &_rctx,
		MessagePointerT &_rmsgptr,
		IpcController &_rctl
	) = 0;
};

struct InitMessage: Dynamic<InitMessage, BaseMessage>{
	InitMessage(){}
	~InitMessage(){}
	
	/*virtual*/ void ipcOnReceive(
		frame::ipc::ConnectionContext const &_rctx,
		MessagePointerT &_rmsgptr,
		IpcController &_rctl
	);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
	}
	
};

struct TextMessage: TextMessageBase, solid::Dynamic<TextMessage, BaseMessage>{
	TextMessage(const std::string &_txt):TextMessageBase(_txt){}
	TextMessage(){}
	
	/*virtual*/ void ipcOnReceive(
		solid::frame::ipc::ConnectionContext const &_rctx,
		MessagePointerT &_rmsgptr,
		IpcController &_rctl
	);
	/*virtual*/ solid::uint32 ipcOnPrepare(solid::frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(solid::frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		TextMessageBase::serialize(_s, _rctx);
	}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		_s.push(text, "text").push(user, "user");
	}
};

struct Handle{
	void beforeSerialization(BinSerializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	bool checkStore(void *, ConnectionContext &_rctx)const{
		return true;
	}
	
	bool checkLoad(void *_pm, ConnectionContext  &_rctx)const{
		return false;//reject all incomming messages
	}
	
	bool checkLoad(LoginRequest *_pm, ConnectionContext  &_rctx)const;
	bool checkLoad(TextMessage *_pm, ConnectionContext  &_rctx)const;
	bool checkLoad(NoopMessage *_pm, ConnectionContext  &_rctx)const;
	
	void afterSerialization(BinSerializerT &_rs, void *_pm, ConnectionContext &_rctx){}
	
	
	void afterSerialization(BinDeserializerT &_rs, LoginRequest *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx);
	
};

namespace{
	struct Params{
		typedef std::vector<SocketAddressInet4>		PeerAddressVectorT;
		typedef std::vector<std::string>			StringVectorT;
		int				chat_port;
		string			chat_addr_str;
		int				ipc_port;
		string			ipc_addr_str;
		StringVectorT	connectstringvec;
		
		
		string			dbg_levels;
		string			dbg_modules;
		string			dbg_addr;
		string			dbg_port;
		bool			dbg_console;
		bool			dbg_buffered;
		bool			log;
		
		PeerAddressVectorT		connectvec;
		
		bool prepare(frame::ipc::Configuration &_rcfg, string &_err);
	};

	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	Params					p;

	static void term_handler(int signum){
		switch(signum) {
			case SIGINT:
			case SIGTERM:{
				if(run){
					
					Locker<Mutex>  lock(mtx);
					run = false;
					cnd.broadcast();
				}
			}
		}
	}
	
}

class Listener;
class Connection;

class Service: public solid::Dynamic<Service, frame::Service>{
public:
	Service(
		frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const serialization::TypeMapperBase &_rtm
	):BaseT(_rm), rsched(_rsched), rtm(_rtm){}
	~Service(){}
private:
	friend class Listener;
	friend class Connection;
	void insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx = NULL,
		bool _secure = false
	);
private:
	AioSchedulerT						&rsched;
	const serialization::TypeMapperBase &rtm;
};

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		Service &_rs,
		const SocketDevice &_rsd,
		frame::aio::openssl::Context *_pctx = NULL
	);
	~Listener();
	/*virtual*/ int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	int									state;
	SocketDevice						sd;
	SslContextPtrT						pctx;
	Service								&rsvc;
};

class IpcController: public frame::ipc::BasicController{
	Service &rchatsvc;
public:
	IpcController(
		Service &_rsvc,
		AioSchedulerT &_rsched,
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	):frame::ipc::BasicController(_rsched, _flags, _resdatasz), rchatsvc(_rsvc){}
	
	IpcController(
		Service &_rsvc,
		AioSchedulerT &_rsched_t,
		AioSchedulerT &_rsched_l,
		AioSchedulerT &_rsched_n,
		const uint32 _flags = 0,
		const uint32 _resdatasz = 0
	):frame::ipc::BasicController(_rsched_t, _rsched_l, _rsched_n, _flags, _resdatasz), rchatsvc(_rsvc){}
		
	Service& chatService()const{
		return rchatsvc;
	}
	/*virtual*/ bool onMessageReceive(
		DynamicPointer<frame::ipc::Message> &_rmsgptr,
		frame::ipc::ConnectionContext const &_rctx
	){
		if(_rmsgptr->isTypeDynamic(BaseMessage::staticTypeId())){
			static_cast<BaseMessage&>(*_rmsgptr).ipcOnReceive(_rctx, _rmsgptr, *this);
		}else{
			_rmsgptr->ipcOnReceive(_rctx, _rmsgptr);
		}
		return true;
	}
};

typedef DynamicPointer<Listener>	ListenerPointerT;

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	if(parseArguments(p, argc, argv)) return 0;
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	/*solid::*/Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask(p.dbg_levels.c_str());
	Debug::the().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Debug::the().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Debug::the().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Debug::the().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 1,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Debug::the().moduleNames(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		frame::Manager			m;
		AioSchedulerT			aiosched(m);
		AioSchedulerT			ipcaiosched(m);
		
		UInt8TypeMapperT		tm;
		
		tm.insertHandle<LoginRequest, Handle>();
		tm.insert<BasicMessage>();
		tm.insertHandle<TextMessage, Handle>();
		tm.insertHandle<NoopMessage, Handle>();
		
		Service					svc(m, aiosched, tm);
		
		frame::ipc::Service		ipcsvc(m, new IpcController(svc, ipcaiosched));
		
		ipcsvc.registerMessageType<InitMessage>();
		ipcsvc.registerMessageType<TextMessage>();
		
		m.registerService(svc);
		m.registerService(ipcsvc);
		
		{
			frame::ipc::Configuration	cfg;
			//solid::Error				err;
			int							err;
			
			{
				string errstr;
				if(!p.prepare(cfg, errstr)){
					cout<<"Error preparing ipc configuration: "<<errstr<<endl;
					Thread::waitAll();
					return 0;
				}
			}
			
			err = ipcsvc.reconfigure(cfg);
			if(err){
				//TODO:
				cout<<"Error starting ipcservice!"<<endl;
				Thread::waitAll();
				return 0;
			}
		}
		
		{
			
			ResolveData		rd =  synchronous_resolve(p.chat_addr_str.c_str(), p.chat_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
	
			sd.create(rd.begin());
			sd.makeNonBlocking();
			sd.prepareAccept(rd.begin(), 100);
			if(!sd.ok()){
				cout<<"Error creating listener"<<endl;
				return 0;
			}
	
			frame::aio::openssl::Context *pctx = NULL;
#if 0
			if(p.secure){
				pctx = frame::aio::openssl::Context::create();
			}
			if(pctx){
				const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
				
				pctx->loadCertificateFile(pcertpath);
				pctx->loadPrivateKeyFile(pcertpath);
			}
#endif
			ListenerPointerT lsnptr(new Listener(svc, sd, pctx));
	
			m.registerObject(*lsnptr);
			aiosched.schedule(lsnptr);
		}
		
		idbg("");
		if(1){
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}else{
			char c;
			cin>>c;
		}
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}
//--------------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("chat-port,p", value<int>(&_par.chat_port)->default_value(2000),"Chat listen port")
			("chat-address", value<string>(&_par.chat_addr_str)->default_value("0.0.0.0"),"Chat listen address")
			("ipc-port,i", value<int>(&_par.ipc_port)->default_value(2010),"IPC Base port")
			("ipc-address", value<string>(&_par.ipc_addr_str)->default_value("localhost"),"IPC listen address")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: YYY.YYY.YYY.YYY:port")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(false)->default_value(true), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
		;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
		if (vm.count("help")) {
			cout << desc << "\n";
			return true;
		}
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}
//------------------------------------------------------
namespace{
bool Params::prepare(frame::ipc::Configuration &_rcfg, string &_err){
	const uint16	default_port = 2000;
	size_t			pos;
	
	for(std::vector<std::string>::iterator it(connectstringvec.begin()); it != connectstringvec.end(); ++it){
		pos = it->rfind(':');
		int port;
		if(pos == std::string::npos){
			port = default_port;
		}else{
			(*it)[pos] = '\0';
			port = atoi(it->c_str() + pos + 1);
		}
		ResolveData	rd = synchronous_resolve(it->c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
		if(!rd.empty()){
			connectvec.push_back(SocketAddressInet4(rd.begin()));
			idbg("added connect address "<<*it<<" "<<connectvec.back());
		}else{
			idbg("skiped connect address "<<*it);
		}
	}
	ResolveData		rd = synchronous_resolve(ipc_addr_str.c_str(), ipc_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
	_rcfg.baseaddr = rd.begin();
	return true;
}
}//namespace
//--------------------------------------------------------------------------
Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd), pctx(_pctx), rsvc(_rsvc){
	state = 0;
}
Listener::~Listener(){
	idbg("");
}
int Listener::execute(ulong, TimeSpec&){
	cassert(this->socketOk());
	if(notified()){
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL) return BAD;
	}
	uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case BAD: return BAD;
				case OK:break;
				case NOK:
					state = 1;
					return NOK;
			}
		}
		state = 0;
		cassert(sd.ok());
		rsvc.insertConnection(sd);
	}
	return OK;
}

//--------------------------------------------------------------------------
class Connection: public solid::Dynamic<Connection, solid::frame::aio::SingleObject>{
	typedef DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
	typedef std::vector<MessageDynamicPointerT>					MessageVectorT;
	//typedef protocol::binary::BasicBufferController<2048>		BufferControllerT;
	typedef protocol::binary::SpecificBufferController<2048>	BufferControllerT;
	enum States{
		StateNotAuth,
		StateAuth,
		StateError,
	};
public:
	
	typedef protocol::binary::AioSession<
		frame::Message,
		int
	>															ProtocolSessionT;
	
	Connection(
		const SocketDevice &_rsd,
		const serialization::TypeMapperBase &_rtm
	):BaseT(_rsd), ser(_rtm), des(_rtm), stt(StateNotAuth){
		idbg((void*)this);
		des.limits().containerlimit = 0;
		des.limits().streamlimit = 0;
		des.limits().stringlimit = 16;
	}
	~Connection(){
		idbg((void*)this);
	}
	ProtocolSessionT &session(){
		return sess;
	}
	bool isAuthenticated()const{
		return stt == StateAuth;
	}
	void onAuthenticate(const std::string &_user);
	void onFailAuthenticate();
	
	Service& service(){
		//NOT nice, but fast
		return static_cast<Service&>(frame::Manager::specific().service(*this));
	}
	const std::string& user()const{
		return usr;
	}
	/*virtual*/ bool notify(DynamicPointer<frame::Message> &_rmsgptr){
		//the mutex is already locked
		msgvec.push_back(_rmsgptr);
		return Object::notify(frame::S_SIG | frame::S_RAISE);
	}
private:
	int done(){
		idbg("");
		bufctl.clear();
		return BAD;
	}
	/*virtual*/ int execute(ulong _sig, TimeSpec &_tout);
private:
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolSessionT		sess;
	BufferControllerT		bufctl;
	uint16					stt;
	std::string				usr;
	MessageVectorT			msgvec;
};

void Service::insertConnection(
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rsd, rtm));
	frame::ObjectUidT rv = this->registerObject(*conptr);
	rsched.schedule(conptr);
}
//--------------------------------------------------------------------------
/*virtual*/ int Connection::execute(ulong _evs, TimeSpec &_tout){
	static Compressor 		compressor(BufferControllerT::DataCapacity);
	
	ulong					sm = grabSignalMask();
	
	idbg((void*)this);
	
	if(sm){
		if(sm & frame::S_KILL) return done();
		if(sm & frame::S_SIG){
			idbg("here");
			Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
			for(MessageVectorT::iterator it(msgvec.begin()); it != msgvec.end(); ++it){
				session().send(0, *it);
			}
			msgvec.clear();
		}
	}
	
	if(_evs & (frame::TIMEOUT | frame::ERRDONE)){
		return done();
	}
	ConnectionContext		ctx(*this);
	int 					rv = BAD;
	switch(stt){
		case StateAuth:
		case StateNotAuth:
			rv = sess.execute(*this, _evs, ctx, ser, des, bufctl, compressor);
			if(rv == NOK){
				_tout.add(20);//wait 20 secs
			}
			if(stt == StateError){
				return OK;
			}
		break;
		case StateError:
			idbg("StateError");
			//Do not read anything from the client, only send the error message
			rv = sess.executeSend(*this, _evs, ctx, ser, bufctl, compressor);
			if(ser.empty() && !socketHasPendingRecv()){//nothing to send
				return done();
			}
			if(rv == NOK){
				_tout.add(10);//short wait for client to read the message
			}
		break;
	}
	return rv;
}
//--------------------------------------------------------------------------
bool Handle::checkLoad(LoginRequest *_pm, ConnectionContext  &_rctx)const{
	return !_rctx.rcon.isAuthenticated();
}
bool Handle::checkLoad(TextMessage *_pm, ConnectionContext  &_rctx)const{
	return _rctx.rcon.isAuthenticated();
}
bool Handle::checkLoad(NoopMessage *_pm, ConnectionContext  &_rctx)const{
	return _rctx.rcon.isAuthenticated();
}

void Handle::afterSerialization(BinDeserializerT &_rs, LoginRequest *_pm, ConnectionContext &_rctx){
	idbg("des:LoginRequest("<<_pm->user<<','<<_pm->pass<<')');
	if(_pm->user.size() && _pm->pass == _pm->user){
		DynamicPointer<frame::Message>	msgptr(new BasicMessage);
		_rctx.rcon.session().send(_rctx.rcvmsgidx, msgptr);
		_rctx.rcon.onAuthenticate(_pm->user);
	}else{
		DynamicPointer<frame::Message>	msgptr(new BasicMessage(BasicMessage::ErrorAuth));
		_rctx.rcon.session().send(_rctx.rcvmsgidx, msgptr);
		_rctx.rcon.onFailAuthenticate();
	}
}

typedef DynamicSharedPointer<TextMessage>	TextMessageSharedPointerT;

struct Notifier{

	const frame::IndexT				objid;
	TextMessageSharedPointerT		msgshrptr;
	
	Notifier(const frame::IndexT &_rid, TextMessage *_pm):objid(_rid), msgshrptr(_pm){}
		
	void operator()(frame::Object &_robj){
		frame::Manager &rm = frame::Manager::specific();
		if(_robj.id() != objid){
			DynamicPointer<frame::Message>	msgptr(msgshrptr);
			if(_robj.notify(msgptr)){
				rm.raise(_robj);
			}
		}
	}
};

void Handle::afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx){
	
	idbg("des:TextMessage("<<_pm->text<<')');
	Notifier		notifier(_rctx.rcon.id(), _pm);
	_pm->user = _rctx.rcon.user();
	_rctx.rcon.service().forEachObject(notifier);
}

void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	idbg("des:NoopMessage");
	//echo back the message itself
	_rctx.rcon.session().send(_rctx.rcvmsgidx, _rctx.rcon.session().recvMessage(_rctx.rcvmsgidx));
}

void Connection::onAuthenticate(const std::string &_user){
	stt = StateAuth;
	des.limits().stringlimit = 1024 * 1024 * 10;
	usr = _user;
}

void Connection::onFailAuthenticate(){
	stt = StateError;
}
//------------------------------------------------------------------------------------
/*virtual*/ void InitMessage::ipcOnReceive(
	frame::ipc::ConnectionContext const &_rctx,
	MessagePointerT &_rmsgptr,
	IpcController &_rctl
){
	
}
/*virtual*/ uint32 InitMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	return 0;
}
/*virtual*/ void InitMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	
}
//------------------------------------------------------------------------------------
/*virtual*/ void TextMessage::ipcOnReceive(
	frame::ipc::ConnectionContext const &_rctx,
	MessagePointerT &_rmsgptr,
	IpcController &_rctl
){
	
}
/*virtual*/ uint32 TextMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	return 0;
}
/*virtual*/ void TextMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	
}
//------------------------------------------------------------------------------------
