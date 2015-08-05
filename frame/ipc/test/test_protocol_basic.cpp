#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcserialization.hpp"

#include "frame/ipc/src/ipcmessagereader.hpp"
#include "frame/ipc/src/ipcmessagewriter.hpp"

using namespace solid;



struct InitStub{
	size_t		size;
	ulong		flags;
};

InitStub initarray[] = {
	{1000, 0},
	{2000, 0},
	{4000, 0},
	{8000, 0},
	{16000, 0},
	{32000, 0},
	{64000, 0},
	{128000, 0},
	{256000, 0},
	{512000, 0},
	{1024000, 0},
	{2048000, 0},
	{4096000, 0},
	{8192000, 0},
	{16384000, 0},
};

std::string						pattern;
const size_t					initarraysize = sizeof(initarray)/sizeof(InitStub);

size_t							crtwriteidx = 0;
size_t							crtreadidx  = 0;
size_t							writecount = 0;


size_t real_size(size_t _sz){
	//offset + (align - (offset mod align)) mod align
	return _sz + ((sizeof(uint64) - (_sz % sizeof(uint64))) % sizeof(uint64));
}

struct Message: Dynamic<Message, frame::ipc::Message>{
	uint32							idx;
    std::string						str;
	
	Message(uint32 _idx):idx(_idx){
		idbg("CREATE ---------------- "<<(void*)this<<" idx = "<<idx);
		init();
		
	}
	Message(){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~Message(){
		idbg("DELETE ---------------- "<<(void*)this);
	}

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(idx, "idx");
		
	}
	
	void init(){
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		str.resize(sz);
		const size_t	count = sz / sizeof(uint64);
		uint64			*pu = reinterpret_cast<uint64*>(const_cast<char*>(str.data()));
		
		for(uint64 i = 0; i < count; ++i){
			pu[i] = pattern[i % pattern.size()];
		}
	}
	bool check()const{
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		if(sz != str.size()){
			return false;
		}
		const size_t	count = sz / sizeof(uint64);
		const uint64	*pu = reinterpret_cast<const uint64*>(str.data());
		for(uint64 i = 0; i < count; ++i){
			if(pu[i] != pattern[i % pattern.size()]) return false;
		}
		return true;
	}
	
};

void receive_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr);
void complete_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr, ErrorConditionT const &_rerr);


namespace solid{namespace frame{namespace ipc{

class TestEntryway{
public:
	static ConnectionContext& createContext(){
		Connection	&rcon = *static_cast<Connection*>(nullptr);
		Service		&rsvc = *static_cast<Service*>(nullptr);
		static ConnectionContext conctx(rsvc, rcon);
		return conctx;
	}
	static void initTypeMap(frame::ipc::TypeIdMapT &_rtm);
};

void TestEntryway::initTypeMap(frame::ipc::TypeIdMapT &_rtm){
	TypeStub ts;
	ts.complete_fnc = MessageCompleteFunctionT(::complete_message);
	ts.receive_fnc = MessageReceiveFunctionT(::receive_message);
	
	_rtm.registerType<::Message>(
		ts,
		Message::serialize<SerializerT, ::Message>,
		Message::serialize<DeserializerT, ::Message>,
		serialization::basic_factory<::Message>
	);
	_rtm.registerCast<::Message, frame::ipc::Message>();
}

}/*namespace ipc*/}/*namespace frame*/}/*namespace solid*/

struct Context{
	Context():ipcconfig(nullptr), ipctypemap(nullptr), ipcmsgreader(nullptr), ipcmsgwriter(nullptr){}
	
	frame::ipc::Configuration		*ipcconfig;
	frame::ipc::TypeIdMapT			*ipctypemap;
	frame::ipc::MessageReader		*ipcmsgreader;
	frame::ipc::MessageWriter		*ipcmsgwriter;
}								ctx;

frame::ipc::ConnectionContext	&ipcconctx(frame::ipc::TestEntryway::createContext());

frame::ipc::MessageWriter& messageWriter(frame::ipc::MessageWriter *_pmsgw = nullptr){
	static frame::ipc::MessageWriter *pmsgw(_pmsgw);
	return *pmsgw;
}


void receive_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr){
	cassert(static_cast<Message&>(*_rmsgptr).check());
	++crtreadidx;
	idbg(crtreadidx);
	if(crtwriteidx < writecount){
		frame::ipc::MessagePointerT	msgptr(new Message(crtwriteidx));
		ctx.ipcmsgwriter->enqueue(msgptr, ctx.ipctypemap->index(msgptr.get()), initarray[crtwriteidx % initarraysize].flags, *ctx.ipcconfig, *ctx.ipctypemap, ipcconctx);
		++crtwriteidx;
	}
}

void complete_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr, ErrorConditionT const &_rerr){
	cassert(!_rerr);
	idbg(static_cast<Message*>(_rmsgptr.get())->idx);
}
 
int test_protocol_basic(int argc, char **argv){
	
	Thread::init();
#ifdef UDEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("all");
	Debug::the().initStdErr(false, nullptr);
#endif
	
	for(int i = 0; i < 127; ++i){
		if(isprint(i) and !isblank(i)){
			pattern += static_cast<char>(i);
		}
	}
	
	const uint16					bufcp(1024*4);
	char							buf[bufcp];
	
	frame::ipc::AioSchedulerT		&ipcsched(*(static_cast<frame::ipc::AioSchedulerT*>(nullptr)));
	frame::ipc::Configuration		ipcconfig(ipcsched);
	frame::ipc::TypeIdMapT			ipctypemap;
	frame::ipc::MessageReader		ipcmsgreader;
	frame::ipc::MessageWriter		ipcmsgwriter;
	
	ErrorConditionT					error;
	
	
	ctx.ipcconfig		= &ipcconfig;
	ctx.ipctypemap		= &ipctypemap;
	ctx.ipcmsgreader	= &ipcmsgreader;
	ctx.ipcmsgwriter	= &ipcmsgwriter;
	
	frame::ipc::TestEntryway::initTypeMap(ipctypemap);
	
	const size_t					start_count = 1;
	
	writecount = start_count;//
	
	for(; crtwriteidx < start_count; ++crtwriteidx){
		frame::ipc::MessagePointerT	msgptr(new Message(crtwriteidx));
		ipcmsgwriter.enqueue(msgptr, ipctypemap.index(msgptr.get()), initarray[crtwriteidx % initarraysize].flags, ipcconfig, ipctypemap, ipcconctx);
	}
	
	
	auto	complete_lambda(
		[](const frame::ipc::MessageReader::Events _event, frame::ipc::MessagePointerT const& _rmsgptr){
			switch(_event){
				case frame::ipc::MessageReader::MessageCompleteE:
					idbg("complete message");
					break;
				case frame::ipc::MessageReader::KeepaliveCompleteE:
					idbg("complete keepalive");
					break;
			}
		}
	);
	
	ipcmsgreader.prepare(ipcconfig);
	
	while(!error){
		uint32 bufsz = ipcmsgwriter.write(buf, bufcp, false, ipcconfig, ipctypemap, ipcconctx, error);
		if(!error){
			frame::ipc::MessageReader::CompleteFunctionT	completefnc(std::cref(complete_lambda));
			
			ipcmsgreader.read(buf, bufsz, completefnc, ipcconfig, ipctypemap, ipcconctx, error);
		}
	}
	
	return 0;
}

