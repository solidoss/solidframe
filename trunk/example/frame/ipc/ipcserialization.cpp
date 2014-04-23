#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"
#include "utility/dynamicpointer.hpp"
#include "utility/dynamictype.hpp"
#include "utility/queue.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "serialization/binary.hpp"
#include "serialization/idtypemapper.hpp"
#include "boost/program_options.hpp"

#include <string>
#include <iostream>

using namespace std;
using namespace solid;

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

struct MessageUid{
	MessageUid(
		const uint32 _idx = 0xffffffff,
		const uint32 _uid = 0xffffffff
	):idx(_idx), uid(_uid){}
	uint32	idx;
	uint32	uid;
};


struct FirstMessage: Dynamic<FirstMessage, DynamicShared<frame::ipc::Message> >{
	uint32			state;
    uint32			sec;
    uint32			nsec;
    std::string		str;
	MessageUid		msguid;
	
	
	FirstMessage();
	~FirstMessage();
	
	uint32 size()const{
		return sizeof(state) + sizeof(sec) + sizeof(nsec) + sizeof(msguid) + sizeof(uint32) + str.size();
	}
	
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s){
		_s.push(state, "state").push(sec, "seconds").push(nsec, "nanoseconds").push(str, "data");
		_s.push(msguid.idx, "msguid.idx").push(msguid.uid,"msguid.uid");
	}
	
};

FirstMessage* create_message(uint32_t _idx, const bool _incremental = false);
namespace{
	Params p;
}
//------------------------------------------------------------------
typedef DynamicSharedPointer<FirstMessage>										FirstMessageSharedPointerT;
typedef Queue<FirstMessageSharedPointerT>										FirstMessageSharedPointerQueueT;
typedef serialization::binary::Serializer<>										BinSerializerT;
typedef serialization::binary::Deserializer<>									BinDeserializerT;
typedef serialization::IdTypeMapper<BinSerializerT, BinDeserializerT, uint16>	UInt16TypeMapperT;


struct Context{
	enum{
		BufferCapacity = 4 * 1024
	};
	Context(): ser(tm), des(tm), bufcp(BufferCapacity){}
	UInt16TypeMapperT	tm;
	BinSerializerT		ser;
	BinDeserializerT	des;
	const size_t		bufcp;
	char				buf[BufferCapacity];
};

void execute_messages(FirstMessageSharedPointerQueueT	&_rmsgq, Context &_ctx);
bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(p, argc, argv)) return 0;
	
	
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
	
	FirstMessageSharedPointerQueueT		msgq;
	Context								ctx;
	ctx.tm.insert<FirstMessage>();
	
	for(uint32 i = 0; i < p.message_count; ++i){
		
		DynamicSharedPointer<FirstMessage>	msgptr(create_message(i));
		msgq.push(msgptr);
	}
	
	TimeSpec	begintime(TimeSpec::createRealTime()); 
	
	for(uint32 i = 0; i < p.repeat_count; ++i){
		execute_messages(msgq, ctx);
	}
	
	TimeSpec	endtime(TimeSpec::createRealTime());
	endtime -= begintime;
	uint64		duration = endtime.seconds() * 1000;
	
	duration += endtime.nanoSeconds() / 1000000;
	
	cout<<"Duration(msec) = "<<duration<<endl;
	
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
/*virtual*/ void FirstMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, FirstMessage::MessagePointerT &_rmsgptr){
	++state;
	idbg("EXECUTE ---------------- "<<state);
}
/*virtual*/ uint32 FirstMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
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

void execute_messages(FirstMessageSharedPointerQueueT	&_rmsgq, Context &_rctx){
	const uint32		sz = _rmsgq.size();
	
	
	for(uint32 i = 0; i < sz; ++i){
		FirstMessageSharedPointerT	frmsgptr = _rmsgq.front();
		FirstMessageSharedPointerT	tomsgptr(new FirstMessage);
		
		_rmsgq.pop();
		
		_rctx.ser.push(*frmsgptr, "msgptr");
		_rctx.des.push(*tomsgptr, "msgptr");
		
		int rv;
		while((rv = _rctx.ser.run(_rctx.buf, _rctx.bufcp)) == _rctx.bufcp){
			_rctx.des.run(_rctx.buf, rv);
		}
		if(rv > 0){
			_rctx.des.run(_rctx.buf, rv);
		}
		
		_rmsgq.push(tomsgptr);
		
	}
}

