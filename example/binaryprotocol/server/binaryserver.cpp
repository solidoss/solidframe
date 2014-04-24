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
#include "example/binaryprotocol/core/messages.hpp"
#include "example/binaryprotocol/core/compressor.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

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


struct Handle{
	void beforeSerialization(BinSerializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	bool checkStore(void *, ConnectionContext &_rctx)const{
		return true;
	}
	
	bool checkLoad(void *_pm, ConnectionContext  &_rctx)const{
		return true;
	}
	void afterSerialization(BinSerializerT &_rs, void *_pm, ConnectionContext &_rctx){}
	
	
	void afterSerialization(BinDeserializerT &_rs, FirstRequest *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, SecondMessage *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx);
	
};

namespace{
	struct Params{
		int			port;
		string		addr_str;
		string		dbg_levels;
		string		dbg_modules;
		string		dbg_addr;
		string		dbg_port;
		bool		dbg_console;
		bool		dbg_buffered;
		bool		log;
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
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	int									state;
	SocketDevice						sd;
	SslContextPtrT						pctx;
	Service								&rsvc;
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
		UInt8TypeMapperT		tm;
		
		tm.insertHandle<FirstRequest, Handle>();
		tm.insert<FirstResponse>();
		tm.insertHandle<SecondMessage, Handle>();
		tm.insertHandle<NoopMessage, Handle>();
		
		Service					svc(m, aiosched, tm);
		m.registerService(svc);
		{
			
			ResolveData		rd =  synchronous_resolve(p.addr_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
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
			("port,p", value<int>(&_par.port)->default_value(2000),"Listen port")
			("address,a", value<string>(&_par.addr_str)->default_value("0.0.0.0"),"Listen address")
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
//--------------------------------------------------------------------------
Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd, true), pctx(_pctx), rsvc(_rsvc){
	state = 0;
}
Listener::~Listener(){
	idbg("");
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
	//typedef protocol::binary::BasicBufferController<2048>		BufferControllerT;
	typedef protocol::binary::SpecificBufferController<2048>	BufferControllerT;
public:
	
	typedef protocol::binary::AioSession<
		frame::Message,
		int
	>															ProtocolSessionT;
	
	Connection(const SocketDevice &_rsd, const serialization::TypeMapperBase &_rtm):BaseT(_rsd), ser(_rtm), des(_rtm){
		idbg((void*)this);
	}
	~Connection(){
		idbg((void*)this);
	}
	ProtocolSessionT &session(){
		return sess;
	}
private:
	void done(){
		idbg("");
		bufctl.clear();
	}
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolSessionT		sess;
	BufferControllerT		bufctl;
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
			//Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
		}
	}
	
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError)){
		done();
		_rexectx.close();
		return;
	}
	ConnectionContext		ctx(*this);
	const AsyncE rv = sess.execute(*this, _rexectx.eventMask(), ctx, ser, des, bufctl, compressor);
	if(rv == solid::AsyncWait){
		_rexectx.waitFor(TimeSpec(20));//wait 20 secs
	}else if(rv == solid::AsyncSuccess){
		_rexectx.reschedule();
	}
}
//--------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, FirstRequest *_pm, ConnectionContext &_rctx){
	idbg("des:FirstRequest("<<_pm->v<<')');
	//echo as FirstResponse
	DynamicPointer<frame::Message>	msgptr(new FirstResponse(_pm->idx, _pm->v));
	_rctx.rcon.session().send(_rctx.rcvmsgidx, msgptr);
}

void Handle::afterSerialization(BinDeserializerT &_rs, SecondMessage *_pm, ConnectionContext &_rctx){
	idbg("des:SecondMessage("<<_pm->v<<')');
	//echo back the message itself
	_rctx.rcon.session().send(_rctx.rcvmsgidx, _rctx.rcon.session().recvMessage(_rctx.rcvmsgidx));
}

void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	idbg("des:NoopMessage");
	//echo back the message itself
	_rctx.rcon.session().send(_rctx.rcvmsgidx, _rctx.rcon.session().recvMessage(_rctx.rcvmsgidx));
}

