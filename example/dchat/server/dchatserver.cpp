#include "protocol/binary/binaryaioengine.hpp"
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
//------------------------------------------------------------------------------------
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

class Connection;
class IpcController;
class Listener;
class Connection;
class Service;

//------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------

typedef serialization::binary::Serializer<ConnectionContext>	BinSerializerT;
typedef serialization::binary::Deserializer<ConnectionContext>	BinDeserializerT;
typedef serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, uint8,
	serialization::FakeMutex
>																UInt8TypeMapperT;

//------------------------------------------------------------------------------------

struct BaseMessage: Dynamic<BaseMessage, DynamicShared<frame::ipc::Message> >{
private:
	friend class IpcController;
	virtual void ipcOnReceive(
		frame::ipc::ConnectionContext const &_rctx,
		MessagePointerT &_rmsgptr,
		IpcController &_rctl
	) = 0;
};

//------------------------------------------------------------------------------------

struct InitMessage: Dynamic<InitMessage, BaseMessage>{
	enum{
		RequestNodes = 1,
		RequestAdd,
		ResponseNodes
	};
	InitMessage():type(0){
	}
	InitMessage(const std::string &_rname, uint8 _type):type(_type){
		nodevec.push_back(_rname);
	}
	~InitMessage(){
	}
	
	/*virtual*/ void ipcOnReceive(
		frame::ipc::ConnectionContext const &_rctx,
		MessagePointerT &_rmsgptr,
		IpcController &_rctl
	);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		_s.pushContainer(nodevec, "nodevec").push(type, "type");
	}
	typedef std::vector<std::string>	StringVectorT;
	uint8			type;
	StringVectorT	nodevec;
};

//------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------

namespace{
	enum NodeTypes{
		LocalNodeType,
		BasicNodeType,
		InitNodeType,
		WaitNodeType
			
	};

	bool split_endpoint_string(std::string &_endpoint, std::string &_addr, int &_port){
		size_t			pos= _endpoint.rfind(':');
		if(pos == std::string::npos){
		}else{
			_endpoint[pos] = '\0';
			_port = atoi(_endpoint.c_str() + pos + 1);
			_addr = _endpoint.c_str();
			_endpoint[pos] = ':';
		}
		return true;
	}

	struct Params{
		//typedef std::vector<SocketAddressInet4>		AddressVectorT;
		typedef std::vector<std::string>			StringVectorT;
		
		int				chat_port;
		string			chat_addr_str;
		string			ipc_endpoint_str;
		StringVectorT	connectstringvec;
		
		
		string			dbg_levels;
		string			dbg_modules;
		string			dbg_addr;
		string			dbg_port;
		bool			dbg_console;
		bool			dbg_buffered;
		bool			log;
		
		//AddressVectorT	connectvec;
		
		bool prepare(frame::ipc::Configuration &_rcfg, Service &_rsvc, string &_err);
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
	
}//namespace

//------------------------------------------------------------------------------------

class Service: public solid::Dynamic<Service, frame::Service>{
	struct NodeStub{
		NodeStub():type(BasicNodeType){}
		std::string				name;
		SocketAddressInet4		addr;
		NodeTypes				type;
	};
	typedef std::vector<NodeStub>	NodeVectorT;
public:
	enum NotifyChoice{
		NotifyAll,
		NotifyInitNodes,
		NotifyWaitNodes,
	};
	Service(
		frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const serialization::TypeMapperBase &_rtm,
		const char *_myname
	):BaseT(_rm), rsched(_rsched), rtm(_rtm){
		nodevec.push_back(NodeStub());
		nodevec.back().name = _myname;
		nodevec.back().type = LocalNodeType;
	}
	~Service(){}
	
	void ipcService(frame::ipc::Service &_ripc){
		pipc = &_ripc;
	}
	frame::ipc::Service& ipcService()const{
		return *pipc;
	}
	std::string myName()const{
		SharedLocker<SharedMutex>	lock(shrmtx);
		return nodevec.front().name;
	}
	bool addNode(std::string &_rname, NodeTypes _type);
	size_t addNodeUnsafe(std::string &_rname, NodeTypes _type);
	void onReceiveMessage(DynamicPointer<InitMessage> &_rmsgptr, frame::ipc::ConnectionContext const &_rctx);
	void onReceiveMessage(DynamicPointer<TextMessage> &_rmsgptr, frame::ipc::ConnectionContext const &_rctx);
	void onNodeDisconnect(SocketAddressInet4 &_raddr);
	
	void notifyNodes(DynamicPointer<frame::ipc::Message> &_rmsgptr, NotifyChoice _choice = NotifyAll);
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
	NodeVectorT							nodevec;
	frame::ipc::Service					*pipc;
	mutable SharedMutex					shrmtx;
};

//------------------------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		Service &_rs,
		const SocketDevice &_rsd,
		frame::aio::openssl::Context *_pctx = NULL
	);
	~Listener();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	int									state;
	SocketDevice						sd;
	SslContextPtrT						pctx;
	Service								&rsvc;
};

//------------------------------------------------------------------------------------

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
	/*virtual*/ void onDisconnect(const SocketAddressInet &_raddr, const uint32 _netid){
		SocketAddressInet4 addr(_raddr);
		chatService().onNodeDisconnect(addr);
	}
};

//------------------------------------------------------------------------------------

typedef DynamicPointer<Listener>	ListenerPointerT;

bool parseArguments(Params &_par, int argc, char *argv[]);

//------------------------------------------------------------------------------------

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
		
		Service					svc(m, aiosched, tm, p.ipc_endpoint_str.c_str());
		
		frame::ipc::Service		ipcsvc(m, new IpcController(svc, ipcaiosched));
		
		ipcsvc.registerMessageType<InitMessage>();
		ipcsvc.registerMessageType<TextMessage>();
		
		svc.ipcService(ipcsvc);
		
		m.registerService(svc);
		m.registerService(ipcsvc);
		
		{
			frame::ipc::Configuration	cfg;
			//solid::Error				err;
			bool						rv;
			
			{
				string errstr;
				if(!p.prepare(cfg, svc, errstr)){
					cout<<"Error preparing ipc configuration: "<<errstr<<endl;
					Thread::waitAll();
					return 0;
				}
			}
			
			rv = ipcsvc.reconfigure(cfg);
			if(!rv){
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
		{
			//send init message to all nodes
			DynamicPointer<frame::ipc::Message>	msgptr(new InitMessage(p.ipc_endpoint_str, InitMessage::RequestNodes));
			svc.notifyNodes(msgptr, Service::NotifyInitNodes);
		}
		vdbg("Wait Ctrl+C to terminate ...");
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
			("ipc-endpoint", value<string>(&_par.ipc_endpoint_str)->default_value("localhost:3000"),"IPC listen address: host_name:port. Must be the same name as the one used for connect!")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: host_name:port. Must be the same name as the one used for ipc-endpoint!")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(false)->default_value(true), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(true)->default_value(false), "Debug unbuffered")
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
bool Params::prepare(frame::ipc::Configuration &_rcfg, Service &_rsvc, string &_err){
	const uint16	default_port = 3000;
	
	std::string	ipc_addr;
	int			ipc_port = default_port;
	
	split_endpoint_string(ipc_endpoint_str, ipc_addr, ipc_port);
	
	ResolveData		rd = synchronous_resolve(ipc_addr.c_str(), ipc_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
	_rcfg.baseaddr = rd.begin();
	
	for(StringVectorT::iterator it(connectstringvec.begin()); it != connectstringvec.end(); ++it){
		_rsvc.addNode(*it, InitNodeType);
	}
	return true;
}
}//namespace
//--------------------------------------------------------------------------
Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd, true), pctx(_pctx), rsvc(_rsvc){
	state = 0;
}
Listener::~Listener(){
}
/*virtual*/ void Listener::execute(ExecuteContext &_rexectx){
	cassert(this->socketOk());
	if(notified()){
		solid::ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	solid::uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case frame::aio::AsyncError:
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:break;
				case frame::aio::AsyncWait:
					state = 1;
					return;
			}
		}
		state = 0;
		cassert(sd.ok());
		rsvc.insertConnection(sd);
	}
	_rexectx.reschedule();
}

//--------------------------------------------------------------------------
class Connection: public solid::Dynamic<Connection, solid::frame::aio::SingleObject>{
	typedef DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
	typedef std::vector<MessageDynamicPointerT>					MessageVectorT;
	typedef Queue<MessageDynamicPointerT>						MessageQueueT;
	//typedef protocol::binary::BasicBufferController<2048>		BufferControllerT;
	typedef protocol::binary::SpecificBufferController<2048>	BufferControllerT;
	enum States{
		StateNotAuth,
		StateAuth,
		StateError,
	};
	struct ProtocolController: solid::protocol::binary::BasicController{
		void onDoneSend(ConnectionContext &_rctx, const size_t _msgidx){
			_rctx.rcon.onDoneSend(_msgidx);
		}
	};
	typedef protocol::binary::AioEngine<
		frame::Message,
		int,
		ProtocolController
	>															ProtocolEngineT;
public:

	
	Connection(
		const SocketDevice &_rsd,
		const serialization::TypeMapperBase &_rtm
	):BaseT(_rsd), ser(_rtm), des(_rtm), stt(StateNotAuth){
		vdbg((void*)this);
		des.limits().containerlimit = 0;
		des.limits().streamlimit = 0;
		des.limits().stringlimit = 16;
	}
	~Connection(){
		vdbg((void*)this);
	}
	ProtocolEngineT &engine(){
		return eng;
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
	void onDoneSend(const size_t _msgidx);
	void done(){
		bufctl.clear();
	}
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	typedef Stack<size_t>	SizeStackT;
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolEngineT			eng;
	BufferControllerT		bufctl;
	uint16					stt;
	std::string				usr;
	MessageVectorT			msgvec;
	MessageQueueT			msgq;
};
//------------------------------------------------------------------------------------
void Service::insertConnection(
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rsd, rtm));
	frame::ObjectUidT rv = this->registerObject(*conptr);
	rsched.schedule(conptr);
}
void Service::onReceiveMessage(DynamicPointer<InitMessage> &_rmsgptr, frame::ipc::ConnectionContext const &_rctx){
	if(_rmsgptr->type == InitMessage::RequestNodes){
		_rmsgptr->type = InitMessage::ResponseNodes;
		std::string		name = _rmsgptr->nodevec.front();
		vdbg("Receive RequestNodes InitMessage "<<name);
		_rmsgptr->nodevec.clear();
		{
			SharedLocker<SharedMutex>	lock(shrmtx);
			for(NodeVectorT::iterator it(nodevec.begin() + 1); it != nodevec.end(); ++it){
				if(it->name != name){
					_rmsgptr->nodevec.push_back(it->name);
					vdbg("addnode "<<it->name);
				}
			}
		}//prevent doble locking with this block
		addNode(name, WaitNodeType);
		DynamicPointer<frame::ipc::Message>	msgptr(_rmsgptr);
		ipcService().sendMessage(msgptr, _rctx.connectionuid);
	}else if(_rmsgptr->type == InitMessage::RequestAdd){
		vdbg("Receive RequestAdd InitMessage "<<_rmsgptr->nodevec.front());
		addNode(_rmsgptr->nodevec.front(), WaitNodeType);
	}else if(_rmsgptr->type == InitMessage::ResponseNodes){
		{
			Locker<SharedMutex>	lock(shrmtx);
			vdbg("Receive ResponseNodes InitMessage{");
			for(InitMessage::StringVectorT::iterator it(_rmsgptr->nodevec.begin()); it != _rmsgptr->nodevec.end(); ++it){
				const size_t idx = addNodeUnsafe(*it, BasicNodeType);
				vdbg(*it<<" on index "<<idx);
				if(idx){
					NodeStub							&rns = nodevec[idx];
					DynamicPointer<frame::ipc::Message>	msgptr(new InitMessage(nodevec[0].name, InitMessage::RequestAdd));
					vdbg("Send message to "<<rns.name);
					ipcService().sendMessage(msgptr, rns.addr);
				}
			}
			vdbg("}");
		}//prevent doble locking with this block
		DynamicPointer<frame::ipc::Message> msgptr(_rmsgptr);
		notifyNodes(msgptr, NotifyWaitNodes);
	}
}
void Service::onReceiveMessage(DynamicPointer<TextMessage> &_rmsgptr, frame::ipc::ConnectionContext const &_rctx){
	idbg(_rmsgptr->text);
	frame::MessageSharedPointerT	msgptr(_rmsgptr);
	this->notifyAll(msgptr);
}
void Service::onNodeDisconnect(SocketAddressInet4 &_raddr){
	idbg(_raddr);
	Locker<SharedMutex>	lock(shrmtx);
	for(NodeVectorT::iterator it(nodevec.begin() + 1); it != nodevec.end(); ++it){
		if(it->addr == _raddr){
			*it = nodevec.back();
			nodevec.pop_back();
			break;
		}
	}
}
void Service::notifyNodes(DynamicPointer<frame::ipc::Message> &_rmsgptr, NotifyChoice _choice){
	SharedLocker<SharedMutex>	lock(shrmtx);
	DynamicSharedPointer<frame::ipc::Message>	shrmsgptr(_rmsgptr);
	for(NodeVectorT::iterator it(nodevec.begin() + 1); it != nodevec.end(); ++it){
		if(_choice == NotifyInitNodes && it->type != InitNodeType){
			continue;
		}
		if(_choice == NotifyWaitNodes && it->type != WaitNodeType){
			continue;
		}
		idbg("Dend message to "<<it->name);
		DynamicPointer<frame::ipc::Message> msgptr(shrmsgptr);
		ipcService().sendMessage(msgptr, it->addr, frame::ipc::LocalNetworkId, frame::ipc::SynchronousSendFlag);
	}
}

size_t Service::addNodeUnsafe(std::string &_rname, NodeTypes _type){
	std::string addr;
	int			port;
	split_endpoint_string(_rname, addr, port);
	
	ResolveData			rd = synchronous_resolve(addr.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
	bool 				found = false;
	SocketAddressInet4	a;
	
	size_t				rv = 0;
	
	a = rd.begin();
	for(NodeVectorT::iterator it(nodevec.begin()); it != nodevec.end(); ++it){
		if(it->addr == a){
			found = true;
			break;
		}
	}
	
	if(!found){
		rv = nodevec.size();
		nodevec.push_back(NodeStub());
		nodevec.back().name = _rname;
		nodevec.back().addr = a;
		nodevec.back().type = _type;
	}
	return rv;
}

bool Service::addNode(std::string &_rname, NodeTypes _type){
	idbg(_rname<<' '<<_type);
	std::string addr;
	int			port;
	split_endpoint_string(_rname, addr, port);
	
	ResolveData			rd = synchronous_resolve(addr.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
	Locker<SharedMutex>	lock(shrmtx);
	bool 				found = false;
	SocketAddressInet4	a;
	a = rd.begin();
	for(NodeVectorT::iterator it(nodevec.begin()); it != nodevec.end(); ++it){
		if(it->addr == a){
			found = true;
			break;
		}
	}
	
	if(!found){
		nodevec.push_back(NodeStub());
		nodevec.back().name = _rname;
		nodevec.back().addr = a;
		nodevec.back().type = _type;
	}
	return true;
}
//--------------------------------------------------------------------------
/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	static Compressor 		compressor(BufferControllerT::DataCapacity);
	
	solid::ulong			sm = grabSignalMask();
	
	if(sm){
		if(sm & frame::S_KILL){
			done();
			_rexectx.close();
			return;
		}
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
			for(MessageVectorT::iterator it(msgvec.begin()); it != msgvec.end(); ++it){
				if(msgq.size() || !engine().isFreeSend(0)){
					msgq.push(*it);
				}else{
					engine().send(0, *it);
				}
			}
			msgvec.clear();
		}
	}
	
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError)){
		done();
		_rexectx.close();
		return;
	}
	ConnectionContext		ctx(*this);
	AsyncE 					rv = AsyncError;
	switch(stt){
		case StateAuth:
		case StateNotAuth:
			rv = engine().run(*this, _rexectx.eventMask(), ctx, ser, des, bufctl, compressor);
			if(rv == solid::AsyncWait){
				_rexectx.waitFor(TimeSpec(60 * 10));
				return;
			}
			if(stt == StateError){
				_rexectx.reschedule();
				return;
			}
			break;
		break;
		case StateError:
			edbg("StateError");
			//Do not read anything from the client, only send the error message
			rv = engine().runSend(*this, _rexectx.eventMask(), ctx, ser, bufctl, compressor);
			if(ser.empty() && !socketHasPendingRecv()){//nothing to send
				done();
				_rexectx.close();
				return;
			}
			if(rv == solid::AsyncWait){
				_rexectx.waitFor(TimeSpec(10));//short wait for client to read the message
				return;
			}
		break;
	}
	if(rv == solid::AsyncSuccess){
		_rexectx.reschedule();
	}else if(rv == solid::AsyncError){
		_rexectx.close();
	}
	return;
}

void Connection::onDoneSend(const size_t _msgidx){
	idbg(_msgidx);
	if(!_msgidx){
		if(msgq.size()){
			engine().send(0, msgq.front());
			msgq.pop();
		}
	}
}

void Connection::onAuthenticate(const std::string &_user){
	stt = StateAuth;
	des.limits().stringlimit = 1024 * 1024 * 10;
	usr = _user;
}

void Connection::onFailAuthenticate(){
	stt = StateError;
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
	idbg("des::LoginRequest("<<_pm->user<<','<<_pm->pass<<')'<<' '<<_rctx.rcvmsgidx);
	if(_pm->user.size() && _pm->pass == _pm->user){
		DynamicPointer<frame::Message>	msgptr(new BasicMessage);
		_rctx.rcon.engine().send(_rctx.rcvmsgidx, msgptr);
		_rctx.rcon.onAuthenticate(_pm->user);
	}else{
		DynamicPointer<frame::Message>	msgptr(new BasicMessage(BasicMessage::ErrorAuth));
		_rctx.rcon.engine().send(_rctx.rcvmsgidx, msgptr);
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
	idbg("des::TextMessage("<<_pm->text<<')'<<' '<<_rctx.rcvmsgidx);
	Notifier		notifier(_rctx.rcon.id(), _pm);
	_pm->user = _rctx.rcon.user();
	_rctx.rcon.service().forEachObject(notifier);
	DynamicPointer<frame::ipc::Message>	msgptr(notifier.msgshrptr);
	_rctx.rcon.service().notifyNodes(msgptr);
}

void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	idbg("des::NoopMessage "<<_rctx.rcvmsgidx);
	//echo back the message itself
	_rctx.rcon.engine().send(_rctx.rcvmsgidx, _rctx.rcon.engine().recvMessage(_rctx.rcvmsgidx));
}
//------------------------------------------------------------------------------------
/*virtual*/ void InitMessage::ipcOnReceive(
	frame::ipc::ConnectionContext const &_rctx,
	MessagePointerT &_rmsgptr,
	IpcController &_rctl
){
	DynamicPointer<InitMessage>	msgptr(_rmsgptr);
	_rctl.chatService().onReceiveMessage(msgptr, _rctx);
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
	DynamicPointer<TextMessage>	msgptr(_rmsgptr);
	_rctl.chatService().onReceiveMessage(msgptr, _rctx);
}
/*virtual*/ uint32 TextMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
	return 0;
}
/*virtual*/ void TextMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
	
}
//------------------------------------------------------------------------------------
