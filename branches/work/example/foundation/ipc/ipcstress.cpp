#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"
#include "foundation/objectselector.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aioobject.hpp"
//#include "foundation/aio/openssl/opensslsocket.hpp"

#include "foundation/ipc/ipcservice.hpp"

//#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
//#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>

using namespace std;

namespace fdt=foundation;

typedef foundation::IndexT									IndexT;
typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;


ostream& operator<<(ostream& _ros, const SocketAddressInet4& _rsa){
	char host[SocketInfo::HostStringCapacity];
	char service[SocketInfo::ServiceStringCapacity];
	_rsa.toString(host, SocketInfo::HostStringCapacity, service, SocketInfo::ServiceStringCapacity, SocketInfo::NumericHost | SocketInfo::NumericService);
	_ros<<host<<':'<<service;
	return _ros;
}

//------------------------------------------------------------------
//------------------------------------------------------------------

struct SocketAddressCmp{
	bool operator()(
		const SocketAddressInet4* const &_pa1,
		const SocketAddressInet4* const &_pa2
	)const{
		return *_pa1 < *_pa2;
	}
};

struct Params{
	typedef std::vector<std::string>			StringVectorT;
	typedef std::vector<SocketAddressInet4>		SocketAddressVectorT;
	typedef std::map<
		const SocketAddressInet4*,
		uint32,
		SocketAddressCmp
	>											SocketAddressMapT;
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	
	int						listen_port;
	bool					log;
	StringVectorT			connectstringvec;
	
	uint32					repeat_count;
    uint32					message_count;
    uint32					min_size;
    uint32					max_size;
    
    
    SocketAddressVectorT	connectvec;
    SocketAddressMapT		connectmap;
	
	void prepare();
    uint32 server(const SocketAddressInet4 &_rsa)const;
};

//------------------------------------------------------------------

struct IpcServiceController: foundation::ipc::Controller{
	IpcServiceController(
		uint32 _netid = 0
	):	foundation::ipc::Controller(400, 0/* AuthenticationFlag*/, 1000, 10 * 1000),
		netid(_netid)
	{
		use();
	}
	
	/*virtual*/ void scheduleTalker(foundation::aio::Object *_po);
	/*virtual*/ void scheduleListener(foundation::aio::Object *_po);
	/*virtual*/ void scheduleNode(foundation::aio::Object *_po);
	
	
	/*virtual*/ uint32 localNetworkId()const{
		return netid;
	}
	/*virtual*/ SocketAddressStub gatewayAddress(
		const uint _idx,
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	){
		return _rsas_dest;
	}
	
	//retval:
	// -1 : wait for asynchrounous event and retry
	// 0: no gateway
	// > 0: the count
	/*virtual*/ int gatewayCount(
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	)const{
		return 1;
	}
private:
	uint32					netid;
};


struct ServerStub{
    ServerStub():minmsec(0xffffffff), maxmsec(0), sz(0){}
    uint64	minmsec;
    uint64	maxmsec;
	uint64	sz;
};

typedef std::vector<ServerStub>     ServerVectorT;

namespace{
	IpcServiceController	ipcctrl;
	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	uint32					wait_count = 0;
	ServerVectorT			srvvec;
	Params					p;
}


struct FirstMessage: Dynamic<FirstMessage, DynamicShared<foundation::Signal> >{
	uint32							state;
    uint32							sec;
    uint32							nsec;
    std::string						str;
	foundation::ipc::SignalUid		siguid;
	
	
	FirstMessage();
	~FirstMessage();
	
	uint32 size()const{
		return sizeof(state) + sizeof(sec) + sizeof(nsec) + sizeof(siguid) + sizeof(uint32) + str.size();
	}
	
	/*virtual*/ void ipcReceive(
		foundation::ipc::SignalUid &_rsiguid
	);
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcComplete(int _err);
	
	bool isOnSender()const{
		return (state % 2) == 0;
	}
	
	template <class S>
	S& operator&(S &_s){
		_s.push(state, "state").push(sec, "seconds").push(nsec, "nanoseconds").push(str, "data");
		if(!isOnSender() || S::IsDeserializer){
			_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalUid &rsiguid(
				const_cast<foundation::ipc::SignalUid &>(foundation::ipc::ConnectionContext::the().signaluid)
			);
			_s.push(rsiguid.idx, "siguid.idx").push(rsiguid.uid,"siguid.uid");
		}
		return _s;
	}
	
};

FirstMessage* create_message(uint32_t _idx, const bool _incremental = false);

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

//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(p, argc, argv)) return 0;
	
	p.prepare();
	
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	
	Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Dbg::instance().levelMask(p.dbg_levels.c_str());
	Dbg::instance().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Dbg::instance().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Dbg::instance().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Dbg::instance().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Dbg::instance().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		
		foundation::Manager 	m(16);
		
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m/*, 0, 6, 1000*/));
		
		
		m.registerService<SchedulerT>(new foundation::Service, 0);
		
		m.registerService<SchedulerT>(new foundation::ipc::Service(ipcctrl.pointer()), 0);
	
		fdt::ipc::Service::the().typeMapper().insert<FirstMessage>();
		
		m.start();
		
		ResolveData rd = synchronous_resolve("0.0.0.0", p.listen_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
		int			rv;
		if(!rd.empty() && !(rv = foundation::ipc::Service::the().insertTalker(rd.begin()))){
			cout<<"[ipc] Added talker on port "<<p.listen_port<<endl;
		}else{
			cout<<"[ipc] Failed adding talker on port "<<p.listen_port<<" rv = "<<rv<<endl;
		}
		
		wait_count = p.message_count;
        
		srvvec.resize(p.connectvec.size());
		
		TimeSpec	begintime(TimeSpec::createRealTime()); 
		
		if(p.connectvec.size()){
			for(uint32 i = 0; i < p.message_count; ++i){
				
				DynamicSharedPointer<FirstMessage>	msgptr(create_message(i));

				for(Params::SocketAddressVectorT::iterator it(p.connectvec.begin()); it != p.connectvec.end(); ++it){
					DynamicPointer<fdt::Signal>		sigptr(msgptr);
					fdt::ipc::Service::the().sendSignal(sigptr, *it, fdt::ipc::LocalNetworkId, fdt::ipc::Service::WaitResponseFlag);
				}
			}
		}
		
		{
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}
		if(srvvec.size()){
			TimeSpec	endtime(TimeSpec::createRealTime());
			endtime -= begintime;
			uint64		duration = endtime.seconds() * 1000;
			
			duration += endtime.nanoSeconds() / 1000000;
			
			uint64		speed = (srvvec.front().sz * 125) / (128 * duration);
			
			cout<<"Speed = "<<speed<<" KB/s"<<endl;
		}
		m.stop();
	}
	Thread::waitAll();
	
	{
        uint64    minmsec = 0xffffffff;
        uint64    maxmsec = 0;
        
        for(ServerVectorT::const_iterator it(srvvec.begin()); it != srvvec.end(); ++it){
            const uint32_t idx = it - srvvec.begin();
            cout<<"Server ["<<p.connectvec[idx]<<"] mintime = "<<it->minmsec<<" maxtime = "<<it->maxmsec<<endl;
            if(minmsec > it->minmsec){
                minmsec = it->minmsec;
            }
            if(maxmsec < it->maxmsec){
                maxmsec = it->maxmsec;
            }
        }
        cout<<"mintime = "<<minmsec<<" maxtime = "<<maxmsec<<endl;
    }
	
	return 0;
}

//------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame ipc stress test");
		desc.add_options()
			("help,h", "List program options")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 9999)")
			("debug_port,P", value<string>(&_par.dbg_port)->default_value("9999"), "Debug server port (e.g. on linux use: nc -l 9999)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,L", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
			("listen_port,l", value<int>(&_par.listen_port)->default_value(2000), "Listen port")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: YYY.YYY.YYY.YYY:port")
			("repeat_count", value<uint32_t>(&_par.repeat_count)->default_value(10), "Per message trip count")
            ("message_count", value<uint32_t>(&_par.message_count)->default_value(10), "Message count")
            ("min_size", value<uint32_t>(&_par.min_size)->default_value(10), "Min message data size")
            ("max_size", value<uint32_t>(&_par.max_size)->default_value(500000), "Max message data size")
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
void Params::prepare(){
	const uint16 default_port = 2000;
	size_t pos;
	for(std::vector<std::string>::iterator it(connectstringvec.begin()); it != connectstringvec.end(); ++it){
		pos = it->find(':');
		if(pos == std::string::npos){
			connectvec.push_back(SocketAddressInet4(it->c_str(), default_port));
		}else{
			(*it)[pos] = '\0';
			int port = atoi(it->c_str() + pos + 1);
			connectvec.push_back(SocketAddressInet4(it->c_str(), port));
		}
		idbg("added connect address "<<connectvec.back());
	}
	if(min_size > max_size){
		uint32_t tmp = min_size;
		min_size = max_size;
		max_size = tmp;
	}
	
	for(SocketAddressVectorT::const_iterator it(connectvec.begin()); it != connectvec.end(); ++it){
		const uint32_t idx = it - connectvec.begin();
		connectmap[&(*it)] = idx;
	}
}
uint32 Params::server(const SocketAddressInet4 &_rsa)const{
	SocketAddressMapT::const_iterator it = this->connectmap.find(&_rsa);
	if(it != this->connectmap.end()){
		return it->second;
	}
	return -1;
}
//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
void IpcServiceController::scheduleTalker(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}

void IpcServiceController::scheduleListener(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}
void IpcServiceController::scheduleNode(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}

//------------------------------------------------------
//		FirstMessage
//------------------------------------------------------

FirstMessage::FirstMessage():state(-1){
	idbg("CREATE ---------------- "<<(void*)this);
}
FirstMessage::~FirstMessage(){
	idbg("DELETE ---------------- "<<(void*)this);
}

/*virtual*/ void FirstMessage::ipcReceive(
	foundation::ipc::SignalUid &_rsiguid
){
	++state;
	idbg("EXECUTE ---------------- "<<state);
	DynamicPointer<fdt::Signal> psig(this);
	if(!isOnSender()){
		fdt::ipc::ConnectionContext::the().service().sendSignal(psig, fdt::ipc::ConnectionContext::the().connectionuid);
	}else{
		TimeSpec			crttime(TimeSpec::createRealTime());
		TimeSpec			tmptime(this->sec, this->nsec);
		SocketAddressInet4	sa(fdt::ipc::ConnectionContext::the().pairaddr);
		
		tmptime =  crttime - tmptime;
		_rsiguid = siguid;
		
		sa.port(fdt::ipc::ConnectionContext::the().baseport);
		
		const uint32		srvidx = p.server(sa);
		ServerStub			&rss = srvvec[srvidx];
		uint64				crtmsec = tmptime.seconds() * 1000;
		
		rss.sz += this->size();
		
		crtmsec += tmptime.nanoSeconds() / 1000000;
		
		if(crtmsec < rss.minmsec){
			rss.minmsec = crtmsec;
		}
		if(crtmsec > rss.maxmsec){
			rss.maxmsec = crtmsec;
		}
		
		if(state <= p.repeat_count){
		
			this->sec = crttime.seconds();
			this->nsec = crttime.nanoSeconds();
			
			fdt::ipc::ConnectionContext::the().service().sendSignal(
				psig,
				fdt::ipc::ConnectionContext::the().connectionuid/*,
				fdt::ipc::Service::WaitResponseFlag*/
			);
		}else{
			Locker<Mutex>  lock(mtx);
			--wait_count;
			idbg("wait_count = "<<wait_count);
			if(wait_count == 0){
				run = false;
				cnd.broadcast();
			}
		}
	}
}
/*virtual*/ uint32 FirstMessage::ipcPrepare(){
	if(isOnSender()){
		return fdt::ipc::Service::WaitResponseFlag;
	}else{
		return 0;
	}
}
/*virtual*/ void FirstMessage::ipcComplete(int _err){
	if(!_err){
        idbg("SUCCESS ----------------");
    }else{
        idbg("ERROR ------------------");
    }
}

string create_string(){
    string s;
    for(char c = '0'; c <= '9'; ++c){
        s += c;
    }
    for(char c = 'a'; c <= 'z'; ++c){
        s += c;
    }
    for(char c = 'A'; c <= 'Z'; ++c){
        s += c;
    }
    return s;
}

FirstMessage* create_message(uint32_t _idx, const bool _incremental){
    static const string s(create_string());
    FirstMessage *pmsg = new FirstMessage;
    
    pmsg->state = 0;
    
    if(!_incremental){
        _idx = p.message_count - 1 - _idx;
    }
    
    const uint32_t size = (p.min_size * (p.message_count - _idx - 1) + _idx * p.max_size) / (p.message_count - 1);
    idbg("create message with size "<<size);
    pmsg->str.resize(size);
    for(uint32_t i = 0; i < size; ++i){
        pmsg->str[i] = s[i % s.size()];
    }
    
    TimeSpec    crttime(TimeSpec::createRealTime());
    pmsg->sec = crttime.seconds();
    pmsg->nsec = crttime.nanoSeconds();
    
    return pmsg;
}