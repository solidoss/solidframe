#include "protocol/binary/client/clientsession.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
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

namespace{
	struct Params{
		int			port;
		string		address_str;
		string		dbg_levels;
		string		dbg_modules;
		string		dbg_addr;
		string		dbg_port;
		bool		dbg_console;
		bool		dbg_buffered;
		bool		log;
		uint32		repeat;
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

class ClientConnection;

struct ConnectionContext{
	ConnectionContext(ClientConnection &_rcon, uint32 _idx):rcon(_rcon), idx(_idx){}
	ClientConnection	&rcon;
	const uint32		idx;
};

typedef serialization::binary::Serializer<ConnectionContext>	BinSerializerT;
typedef serialization::binary::Deserializer<ConnectionContext>	BinDeserializerT;
typedef serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, uint8,
	serialization::FakeMutex
>																UInt8TypeMapperT;

class ClientConnection: public frame::aio::SingleObject{
	
	typedef DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
	typedef std::pair<MessageDynamicPointerT, uint32>			MessagePairT;
	typedef std::vector<MessagePairT>							MessageVectorT;
	typedef protocol::binary::client::Session					ProtocolSessionT;
		
	enum{
		RecvBufferCapacity = 1024 * 4,
		SendBufferCapacity = 1024 * 4,
		DataCapacity = SendBufferCapacity >> 1,
	};
	enum{
		InitState = 1,
		PrepareState,
		ConnectState,
		ConnectWaitState,
		RunningState
	};
public:
	ClientConnection(
		frame::Manager &_rm,
		const ResolveData &_rd,
		const serialization::TypeMapperBase &_rtm
	):rm(_rm), rd(_rd), st(PrepareState), ser(_rtm), des(_rtm){}
	
	void send(DynamicPointer<solid::frame::Message>	&_rmsgptr, const uint32 _flags = 0){
		Locker<Mutex>	lock(rm.mutex(*this));
		
		sndmsgvec.push_back(MessagePairT(_rmsgptr, _flags));
		
		if(Object::notify(frame::S_SIG | frame::S_RAISE)){
			rm.raise(*this);
		}
	}
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<FirstResponse> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<SecondResponse> &_rmsgptr);
private:
	/*virtual*/ int execute(ulong _evs, TimeSpec& _crtime);
	int done(){
		idbg("");
		Locker<Mutex>	lock(mtx);
		run = false;
		cnd.signal();
		return BAD;
	}
private:
	frame::Manager			&rm;
	ResolveData				rd;
	uint16					st;
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolSessionT		session;
	MessageVectorT			sndmsgvec;
	char					recvbuf[RecvBufferCapacity];
	char					sendbuf[SendBufferCapacity];
};

bool parseArguments(Params &_par, int argc, char *argv[]);

struct Handle{
	bool checkStore(void *, ConnectionContext *_pctx)const{
		return true;
	}
	
	bool checkLoad(FirstResponse *_pm, ConnectionContext* _pctx)const{
		return true;
	}
	void handle(FirstResponse *_pm, ConnectionContext *_pctx, const char *_name){
		cout<<_name<<endl;
	}
	bool checkLoad(SecondResponse *_pm, ConnectionContext* _pctx)const{
		return true;
	}
	void handle(SecondResponse *_pm, ConnectionContext *_pctx, const char *_name){
		cout<<_name<<endl;
	}
};


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
	for(uint32 i = 0; i < p.repeat; ++i){
		vdbgx(Debug::aio, i<<" verbose message verbose message verbose message verbose message verbose message verbose message verbose message ");
		idbgx(Debug::aio, i<<" info message info message info message info message info message info message info message info message");
		edbgx(Debug::aio, i<<" error message error message error message error message error message error message error message error message ");
		wdbgx(Debug::aio, i<<" warning message warning message warning message warning message warning message warning message warning message ");
		
		vdbg(i<<" verbose message verbose message verbose message verbose message verbose message verbose message verbose message ");
		idbg(i<<" info message info message info message info message info message info message info message info message");
		edbg(i<<" error message error message error message error message error message error message error message error message ");
		wdbg(i<<" warning message warning message warning message warning message warning message warning message warning message ");
	}
	
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		UInt8TypeMapperT						tm;
		
		tm.insert<FirstRequest>();
		tm.insertHandle<FirstResponse, Handle>();
		tm.insert<SecondRequest>();
		tm.insertHandle<SecondResponse, Handle>();
		
		
		ClientConnectionPointerT				ccptr(
			new ClientConnection(
				m,
				synchronous_resolve(p.address_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream),
				tm
			)
		);
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<solid::frame::Message>	msgptr;
		
		msgptr = new FirstRequest(10);
		
		ccptr->send(msgptr);
		
		msgptr = new SecondRequest("Hello world!!");
		
		ccptr->send(msgptr);
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

//-----------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("port,p", value<int>(&_par.port)->default_value(2000),"Server port")
			("repeat,r", value<uint32>(&_par.repeat)->default_value(0),"Repeat")
			("address,a", value<string>(&_par.address_str)->default_value("localhost"),"Server address")
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

//-----------------------------------------------------------------------

/*virtual*/ int ClientConnection::execute(ulong _evs, TimeSpec& _crtime){
	static Compressor 		compressor(DataCapacity);
	
	ulong sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL) return done();
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(rm.mutex(*this));
			session.schedule(sndmsgvec.begin(), sndmsgvec.end());
			sndmsgvec.clear();
		}
	}
	if(_evs & frame::ERRDONE){
		idbg("ioerror "<<_evs<<' '<<socketEventsGrab());
		return done();
	}
	if(_evs & frame::INDONE){
		idbg("indone");
		char	tmpbuf[DataCapacity];
		if(!session.consume(recvbuf, this->socketRecvSize(), compressor, tmpbuf, DataCapacity)){
			return done();
		}
	}
	bool reenter = false;
	if(st == RunningState){
		idbg("RunningState");
		if(!this->socketHasPendingRecv()){
			switch(this->socketRecv(session.recvBufferOffset(recvbuf), session.recvBufferCapacity(RecvBufferCapacity))){
				case BAD: return done();
				case OK:{
					char	tmpbuf[DataCapacity];
					if(!session.consume(recvbuf, this->socketRecvSize(), compressor, tmpbuf, DataCapacity)){
						return done();
					}
					reenter = true;
				}break;
				default:
					break;
			}
		}
		if(!this->socketHasPendingSend()){
			int		cnt = 4;
			char	tmpbuf[DataCapacity];
			while((cnt--) > 0){
				int rv = session.fill(sendbuf, SendBufferCapacity, compressor, tmpbuf, DataCapacity);
				if(rv == BAD) return done();
				if(rv == NOK) break;
				switch(this->socketSend(sendbuf, rv)){
					case BAD: 
						return done();
					case NOK:
						cnt = 0;
						break;
					default:
						break;
				}
			}
			if(cnt == 0){
				reenter = true;
			}
		}
	}else if(st == PrepareState){
		SocketDevice	sd;
		sd.create(rd.begin());
		sd.makeNonBlocking();
		socketInsert(sd);
		socketRequestRegister();
		st = ConnectState;
		reenter = false;
	}else if(st == ConnectState){
		idbg("ConnectState");
		switch(socketConnect(rd.begin())){
			case BAD: return done();
			case OK:
				idbg("");
				st = InitState;
				reenter = true;
				break;
			case NOK:
				st = ConnectWaitState;
				idbg("");
				break;
		}
	}else if(st == ConnectWaitState){
		idbg("ConnectWaitState");
		st = InitState;
		reenter = true;
	}else if(st == InitState){
		idbg("InitState");
		st = RunningState;
		reenter = true;
	}
	return reenter ? OK : NOK;
}
