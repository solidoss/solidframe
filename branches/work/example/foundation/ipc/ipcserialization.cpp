#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/dynamictype.hpp"
#include "foundation/signal.hpp"

#include "boost/program_options.hpp"

#include <string>
#include <iostream>

namespace fdt=foundation;

using namespace std;

struct Params{
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	bool					log;
	
	uint32					repeat_count;
    uint32					message_count;
    uint32					min_size;
    uint32					max_size;
};

struct SignalUid{
	SignalUid(
		const uint32 _idx = 0xffffffff,
		const uint32 _uid = 0xffffffff
	):idx(_idx), uid(_uid){}
	uint32	idx;
	uint32	uid;
};


struct FirstMessage: Dynamic<FirstMessage, DynamicShared<foundation::Signal> >{
	uint32			state;
    uint32			sec;
    uint32			nsec;
    std::string		str;
	SignalUid		siguid;
	
	
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
		_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		return _s;
	}
	
};

FirstMessage* create_message(uint32_t _idx, const bool _incremental = false);
namespace{
	Params p;
}
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(p, argc, argv)) return 0;
	
	
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
	
	for(uint32 i = 0; i < p.message_count; ++i){
		
		DynamicSharedPointer<FirstMessage>	msgptr(create_message(i));

	}
	Thread::waitAll();
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
			("repeat_count", value<uint32_t>(&_par.repeat_count)->default_value(10), "Per message trip count")
            ("message_count", value<uint32_t>(&_par.message_count)->default_value(1000), "Message count")
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
}
/*virtual*/ uint32 FirstMessage::ipcPrepare(){
	return 0;
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


