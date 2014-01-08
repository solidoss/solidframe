#include "frame/manager.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aioobject.hpp"
//#include "foundation/aio/openssl/opensslsocket.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcmessage.hpp"

//#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
//#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>

using namespace std;
using namespace solid;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;



//------------------------------------------------------------------
//------------------------------------------------------------------

typedef std::pair<uint32, SocketAddressInet4>	PeerAddressPairT;

struct SocketAddressCmp{
	bool operator()(
		const PeerAddressPairT* const &_pa1,
		const PeerAddressPairT* const &_pa2
	)const{
		if(_pa1->first < _pa2->first){
			return true;
		}else if(_pa1->first > _pa2->first){
			return false;
		}else{
			return  *_pa1 < *_pa2;
		}
	}
};

struct Params{
	typedef std::vector<std::string>			StringVectorT;
	typedef std::vector<PeerAddressPairT>		PeerAddressVectorT;
	typedef std::map<
		const PeerAddressPairT*,
		uint32,
		SocketAddressCmp
	>											SocketAddressMapT;
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	
	int						baseport;
	bool					log;
	StringVectorT			connectstringvec;
	StringVectorT			gatewaystringvec;
	
	uint32					repeat_count;
    uint32					message_count;
    uint32					min_size;
    uint32					max_size;
	uint32					netid;
    
    
    PeerAddressVectorT		connectvec;
    SocketAddressMapT		connectmap;
	
	bool prepare(frame::ipc::Configuration &_rcfg, string &_err);
    uint32 server(const PeerAddressPairT&)const;
};

//------------------------------------------------------------------

struct ServerStub{
    ServerStub():minmsec(0xffffffff), maxmsec(0), sz(0){}
    uint64	minmsec;
    uint64	maxmsec;
	uint64	sz;
};


struct MessageStub{
	MessageStub():count(0){}
	uint32 count;
};

typedef std::vector<ServerStub>     ServerVectorT;
typedef std::vector<MessageStub>    MessageVectorT;

namespace{
	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	uint32					wait_count = 0;
	ServerVectorT			srvvec;
	MessageVectorT			msgvec;
	Params					p;
}


struct FirstMessage: Dynamic<FirstMessage, DynamicShared<frame::ipc::Message> >{
	uint32							idx;
    uint32							sec;
    uint32							nsec;
	int								state;
    std::string						str;
	frame::ipc::MessageUid			msguid;
	
	
	FirstMessage();
	~FirstMessage();
	
	uint32 size()const{
		return sizeof(sec) + sizeof(nsec) + sizeof(msguid) + sizeof(uint32) + str.size();
	}
	
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		_s.push(state, "state").push(idx, "index").push(sec, "seconds").push(nsec, "nanoseconds").push(str, "data");
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
	
	//p.prepare();
	
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	
	Thread::init();
	
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
			1024 * 1024 * 64,
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
		
		frame::ipc::Service		ipcsvc(m, new frame::ipc::BasicController(aiosched));
		
		ipcsvc.registerMessageType<FirstMessage>();
		
		m.registerService(ipcsvc);
		
		{
			frame::ipc::Configuration	cfg;
			ResolveData					rd = synchronous_resolve("0.0.0.0", p.baseport, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			//frame::aio::Error			err;
			bool						rv;
			
			{
				string errstr;
				if(!p.prepare(cfg, errstr)){
					cout<<"Error preparing ipc configuration: "<<errstr<<endl;
					Thread::waitAll();
					return 0;
				}
			}
			
			
			cfg.baseaddr = rd.begin();
			
			rv = ipcsvc.reconfigure(cfg);
			if(!rv){
				//TODO:
				//cout<<"Error starting ipcservice: "<<err.toString()<<endl;
				Thread::waitAll();
				return 0;
			}
		}
		
		
		wait_count = p.message_count;
        
		srvvec.resize(p.connectvec.size());
		msgvec.resize(p.message_count);
		
		TimeSpec	begintime(TimeSpec::createRealTime()); 
		
		if(p.connectvec.size()){
			for(uint32 i = 0; i < p.message_count; ++i){
				
				DynamicSharedPointer<FirstMessage>	fmsgptr(create_message(i));

				for(Params::PeerAddressVectorT::iterator it(p.connectvec.begin()); it != p.connectvec.end(); ++it){
					DynamicPointer<frame::ipc::Message>		msgptr(fmsgptr);
					ipcsvc.sendMessage(
						msgptr, it->second,
						it->first,
						frame::ipc::WaitResponseFlag// | fdt::ipc::Service::SynchronousSendFlag
					);
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
		vdbg("done stop");
	}
	Thread::waitAll();
	
	{
        uint64    minmsec = 0xffffffff;
        uint64    maxmsec = 0;
        
        for(ServerVectorT::const_iterator it(srvvec.begin()); it != srvvec.end(); ++it){
            const uint32_t idx = it - srvvec.begin();
            cout<<"Server ["<<p.connectvec[idx].first<<':'<<p.connectvec[idx].second<<"] mintime = "<<it->minmsec<<" maxtime = "<<it->maxmsec<<endl;
            if(minmsec > it->minmsec){
                minmsec = it->minmsec;
            }
            if(maxmsec < it->maxmsec){
                maxmsec = it->maxmsec;
            }
        }
        cout<<"mintime = "<<minmsec<<" maxtime = "<<maxmsec<<endl;
		bool	doprint = false;
		uint32	cnt(0);
		if(msgvec.size() <= 16){
			doprint = true;
		}else{
			cnt = msgvec.front().count;
			
			for(MessageVectorT::const_iterator it(msgvec.begin() + 1); it != msgvec.end(); ++it){
				if(it->count != cnt){
					doprint = true;
					break;
				}
			}
		}
		if(doprint){
			for(MessageVectorT::const_iterator it(msgvec.begin()); it != msgvec.end(); ++it){
				size_t idx = it - msgvec.begin();
				cout<<idx<<'('<<it->count<<')'<<' ';
			}
			cout<<endl;
		}else{
			cout<<"All "<<msgvec.size()<<" messages have count: "<<cnt<<endl;
		}
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
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 9999)")
			("debug-port,P", value<string>(&_par.dbg_port)->default_value("9999"), "Debug server port (e.g. on linux use: nc -l 9999)")
			("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("base-port,b", value<int>(&_par.baseport)->default_value(2000), "IPC Base/Listen port")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: netid:YYY.YYY.YYY.YYY:port")
			("gateway,g", value<vector<string> >(&_par.gatewaystringvec), "Gateways to connect through: YYY.YYY.YYY.YYY:port")
			("repeat-count", value<uint32_t>(&_par.repeat_count)->default_value(10), "Per message trip count")
            ("message-count", value<uint32_t>(&_par.message_count)->default_value(1000), "Message count")
            ("min-size", value<uint32_t>(&_par.min_size)->default_value(10), "Min message data size")
            ("max-size", value<uint32_t>(&_par.max_size)->default_value(500000), "Max message data size")
			("netid,n", value<uint32>(&_par.netid), "Network identifier")
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
bool Params::prepare(frame::ipc::Configuration &_rcfg, string &_err){
	const uint16	default_gw_port = 4000;
	const uint16	default_port = 2000;
	const int		default_netid = solid::frame::ipc::LocalNetworkId;
	size_t			pos;
	
	for(std::vector<std::string>::iterator it(gatewaystringvec.begin()); it != gatewaystringvec.end(); ++it){
		pos = it->rfind(':');
		if(pos == std::string::npos){
			//connectvec.push_back(SocketAddressInet4(it->c_str(), default_port));
			_rcfg.gatewayaddrvec.push_back(SocketAddressInet(it->c_str(), default_gw_port));
		}else{
			(*it)[pos] = '\0';
			int port = atoi(it->c_str() + pos + 1);
			_rcfg.gatewayaddrvec.push_back(SocketAddressInet(it->c_str(), port));
		}
		idbg("added gateway address "<<_rcfg.gatewayaddrvec.back());
	}

	size_t 			posa;
	size_t 			posb;
	for(std::vector<std::string>::iterator it(connectstringvec.begin()); it != connectstringvec.end(); ++it){
		posa = it->find(':');
		posb = it->rfind(':');
		int netid = default_netid;
		int port  = -1;
		
		
		if(posa == std::string::npos){
			_err = "Error parsing connect address: ";
			_err += *it;
			return false;
		}
		
		(*it)[posa] = '\0';
		netid = atoi(it->c_str());
	
		if(posb == posa){
			port = default_port;
		}else{
			(*it)[posb] = '\0';
			port = atoi(it->c_str() + posb + 1);
		}
		
		const char *addr = it->c_str() + posa + 1;
		
		ResolveData	rd = synchronous_resolve(addr, port, 0, SocketInfo::Inet4, SocketInfo::Stream);
		
		if(!rd.empty()){
			connectvec.push_back(PeerAddressPairT(netid, SocketAddressInet4(rd.begin())));
			idbg("added connect address "<<*it<<" "<<connectvec.back().first<<":"<<connectvec.back().second);
		}else{
			idbg("skiped connect address "<<*it);
		}
	}
	
	if(min_size > max_size){
		uint32_t tmp = min_size;
		min_size = max_size;
		max_size = tmp;
	}
	
	for(PeerAddressVectorT::const_iterator it(connectvec.begin()); it != connectvec.end(); ++it){
		const uint32_t idx = it - connectvec.begin();
		connectmap[&(*it)] = idx;
	}
	_rcfg.localnetid = netid;
	return true;
}
uint32 Params::server(const PeerAddressPairT &_rsa)const{
	SocketAddressMapT::const_iterator it = this->connectmap.find(&_rsa);
	if(it != this->connectmap.end()){
		return it->second;
	}
	return -1;
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

/*virtual*/ void FirstMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr){
	++state;
	idbg("EXECUTE ---------------- "<<state<<" size = "<<str.size());
	
	if(this->idx >= msgvec.size()){
		msgvec.resize(this->idx + 1);
	}
	
	++msgvec[this->idx].count;
	
	if(ipcIsOnReceiver()){
		
		PeerAddressPairT	peersa(_rctx.netid, _rctx.pairaddr);
		
		idbg("Received message: "<<peersa.first<<":"<<peersa.second);
		
		_rctx.service().sendMessage(_rmsgptr, _rctx.connectionuid, (uint32)0/*fdt::ipc::Service::SynchronousSendFlag*/);
	}else{
		TimeSpec			crttime(TimeSpec::createRealTime());
		TimeSpec			tmptime(this->sec, this->nsec);
		PeerAddressPairT	peersa(_rctx.netid, _rctx.pairaddr);
		
		idbg("Received message: "<<peersa.first<<":"<<peersa.second<<" waiting message: "<<(void*)_rctx.requestMessage(*this).get());
		
		tmptime =  crttime - tmptime;
		
		peersa.second.port(_rctx.baseport);
		
		const uint32		srvidx = p.server(peersa);
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
			
			_rctx.service().sendMessage(_rmsgptr, _rctx.connectionuid, frame::ipc::WaitResponseFlag/*frame::ipc::Service::SynchronousSendFlag*/);
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
/*virtual*/ uint32 FirstMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
// 	if(isOnSender()){
// 		return frame::ipc::WaitResponseFlag;
// 	}else{
// 		return 0;
// 	}
	return 0;
}
/*virtual*/ void FirstMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
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
	pmsg->idx = _idx;
    
    if(!_incremental){
        _idx = p.message_count - 1 - _idx;
    }
    
    const uint32_t size = p.message_count == 1 ? p.min_size : (p.min_size * (p.message_count - _idx - 1) + _idx * p.max_size) / (p.message_count - 1);
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