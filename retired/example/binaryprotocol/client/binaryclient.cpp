#include "protocol/binary/binaryaioengine.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"

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
	string					prefix;
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

struct Handle;


class Connection: public frame::aio::SingleObject{
	typedef DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
	struct MessageStub{
		MessageStub():flags(0), idx(-1){}
		MessageStub(
			MessageDynamicPointerT	&_rmsgptr,
			const uint32 _flags = 0,
			const size_t _idx = -1
		):msgptr(_rmsgptr), flags(_flags), idx(_idx){}
		
		MessageDynamicPointerT	msgptr;
		uint32					flags;
		size_t					idx;
	};
	
	typedef std::vector<MessageStub>							MessageVectorT;
	typedef protocol::binary::AioEngine<
		frame::Message,
		int
	>															ProtocolEngineT;
		
	typedef protocol::binary::BasicBufferController<2048>		BufferControllerT;
	enum{
		InitState = 1,
		PrepareState,
		ConnectState,
		ConnectWaitState,
		RunningState
	};
public:
	Connection(
		frame::Manager &_rm,
		const ResolveData &_rd,
		const serialization::TypeMapperBase &_rtm
	):rm(_rm), rd(_rd), st(PrepareState), ser(_rtm), des(_rtm), sndidx(1), waitnoop(false){}
	
	size_t send(DynamicPointer<solid::frame::Message>	&_rmsgptr, const uint32 _flags = 0){
		Locker<Mutex>	lock(rm.mutex(*this));
		size_t idx;
		
		if(freesndidxstk.size()){
			idx = freesndidxstk.top();
			freesndidxstk.pop();
		}else{
			idx = sndidx;
			++sndidx;
		}
		
		sndmsgvec.push_back(MessageStub(_rmsgptr, _flags, idx));
		
		if(Object::notify(frame::S_SIG | frame::S_RAISE)){
			rm.raise(*this);
		}
		return idx;
	}
	
private:
	friend struct Handle;
	friend struct EngineController;
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	void done(){
		idbg("");
		cout<<"Connection closed"<<endl;
		Locker<Mutex>	lock(mtx);
		run = false;
		cnd.signal();
	}
	void onReceiveNoop(){
		cassert(waitnoop);
		waitnoop = false;
	}
	void onDoneIndex(uint32 _msgidx){
		Locker<Mutex>	lock(rm.mutex(*this));
		freesndidxstk.push(_msgidx);
	}
private:
	typedef Stack<size_t>	SizeStackT;
	
	frame::Manager			&rm;
	ResolveData				rd;
	uint16					st;
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolEngineT			engine;
	MessageVectorT			sndmsgvec;
	BufferControllerT		bufctl;
	size_t					sndidx;
	bool					waitnoop;
	SizeStackT				freesndidxstk;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

struct Handle{
	void beforeSerialization(BinSerializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	bool checkStore(void *, ConnectionContext &_rctx)const{
		return true;
	}
	
	bool checkLoad(void *_pm, ConnectionContext &_rctx)const{
		return true;
	}
	void afterSerialization(BinSerializerT &_rs, void *_pm, ConnectionContext &_rctx){}
	
	void afterSerialization(BinDeserializerT &_rs, FirstResponse *_pm, ConnectionContext &_rctx);
		
	void afterSerialization(BinDeserializerT &_rs, SecondMessage *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx);
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
	{
		typedef DynamicSharedPointer<Connection>	ConnectionPointerT;
		
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		UInt8TypeMapperT						tm;
		
		tm.insert<FirstRequest>();
		tm.insertHandle<FirstResponse, Handle>();
		tm.insertHandle<SecondMessage, Handle>();
		tm.insertHandle<NoopMessage, Handle>();
		
		
		ConnectionPointerT				ccptr(
			new Connection(
				m,
				synchronous_resolve(p.address_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream),
				tm
			)
		);
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<solid::frame::Message>	msgptr;
		uint32									idx = 0;
		
		
		do{
			if(idx % 2){
				{
					Locker<Mutex> lock(mtx);
					prefix = "number[0 to exit] >";
					cout<<prefix<<flush;
				}
				uint64 no;
				cin>>no;
				if(no == 0) break;
				msgptr = new FirstRequest(idx++, no);
				ccptr->send(msgptr);
			}else{
				{
					Locker<Mutex> lock(mtx);
					prefix = "string[q to exit] >";
					cout<<prefix<<flush;
				}
				string s;
				cin>>s;
				if((s.size() == 1) && (s[0] == 'q' || s[0] == 'Q')) break;
				msgptr = new SecondMessage(idx++, s);
				ccptr->send(msgptr);
			}
		
		}while(true);
		
		idbg("");
		
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
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value(""),"Debug logging modules")
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
/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	typedef DynamicSharedPointer<solid::frame::Message>		MessageSharedPointerT;
	
	static Compressor 				compressor(BufferControllerT::DataCapacity);
	static MessageSharedPointerT	noopmsgptr(new NoopMessage);
	
	solid::ulong					sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL){
			done();
			_rexectx.close();
			return;
		}
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(rm.mutex(*this));
			for(MessageVectorT::iterator it(sndmsgvec.begin()); it != sndmsgvec.end(); ++it){
				engine.send(it->idx, it->msgptr, it->flags);
			}
			sndmsgvec.clear();
		}
	}
	solid::ulong					evs = _rexectx.eventMask();
	if(evs & (frame::EventTimeout | frame::EventDoneError)){
		if(waitnoop){
			done();
			_rexectx.close();
			return;
		}else if(engine.isSendQueueEmpty()){
			DynamicPointer<solid::frame::Message>	msgptr(noopmsgptr);
			send(msgptr);
			waitnoop = true;
		}
	}
	
	ConnectionContext				ctx(*this);
	
	bool reenter = false;
	if(st == RunningState){
		const AsyncE rv = engine.run(*this, evs, ctx, ser, des, bufctl, compressor);
		if(rv == AsyncWait){
			if(waitnoop){
				_rexectx.waitFor(TimeSpec(3));
			}else{
				_rexectx.waitFor(TimeSpec(15));//wait 15 secs then send noop
			}
		}else if(rv == AsyncSuccess){
			_rexectx.reschedule();
		}else{
			done();
			_rexectx.close();
		}
		return;
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
			case frame::aio::AsyncSuccess:
				idbg("");
				st = InitState;
				reenter = true;
				break;
			case frame::aio::AsyncWait:
				st = ConnectWaitState;
				idbg("");
				break;
			case frame::aio::AsyncError:
				done();
				_rexectx.close();
				return;
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
	if(reenter){
		_rexectx.reschedule();
	}
}
//---------------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, FirstResponse *_pm, ConnectionContext &_rctx){
	static const char *blancs = "                                    ";
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	{
		Locker<Mutex> lock(mtx);
		cout<<'\r'<<blancs<<'\r'<<_rctx.rcvmsgidx<<' '<<_pm->idx<<' '<<_pm->v<<endl;
		cout<<prefix<<flush;
	}
}
	
void Handle::afterSerialization(BinDeserializerT &_rs, SecondMessage *_pm, ConnectionContext &_rctx){
	static const char *blancs = "                                    ";
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	{
		Locker<Mutex> lock(mtx);
		cout<<"\r"<<blancs<<'\r'<<_rctx.rcvmsgidx<<' '<<_pm->idx<<' '<<_pm->v<<endl;
		cout<<prefix<<flush;
	}
}
void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	_rctx.rcon.onReceiveNoop();
}
//---------------------------------------------------------------------------------
