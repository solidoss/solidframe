#include "test_protocol_common.hpp"
#include "system/exception.hpp"
#include "frame/ipc/ipcerror.hpp"

using namespace solid;


namespace{

struct InitStub{
	size_t		size;
	bool		cancel;
	ulong		flags;
};

InitStub initarray[] = {
	{100000,	false,	0},//first message must not be canceled
	{16384000,	false,	0},//not caceled
	{8192000,	true,	frame::ipc::Message::SynchronousFlagE},
	{4096000,	true,	0},
	{2048000,	false,	0},//not caceled
	{1024000,	true,	0},
	{512000,	false,	frame::ipc::Message::SynchronousFlagE},//not canceled
	{256000,	true,	0},
	{128000,	true,	0},
	{64000,		true,	0},
	{32000,		false,	0},//not canceled
	{16000,		true,	0},
	{8000,		true,	0},
	{4000,		true,	0},
	{2000,		true,	0},
};

typedef std::vector<frame::ipc::MessageUid>		MessageUidVectorT;

std::string						pattern;
const size_t					initarraysize = sizeof(initarray)/sizeof(InitStub);

size_t							crtwriteidx = 0;
size_t							crtreadidx  = 0;
size_t							writecount = 0;

MessageUidVectorT				message_uid_vec;


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
		uint64			*pu  = reinterpret_cast<uint64*>(const_cast<char*>(str.data()));
		const uint64	*pup = reinterpret_cast<const uint64*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64);
		for(uint64 i = 0; i < count; ++i){
			pu[i] = pup[i % pattern_size];//pattern[i % pattern.size()];
		}
	}
	bool check()const{
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		idbg("str.size = "<<str.size()<<" should be equal to "<<sz);
		if(sz != str.size()){
			return false;
		}
		const size_t	count = sz / sizeof(uint64);
		const uint64	*pu = reinterpret_cast<const uint64*>(str.data());
		const uint64	*pup = reinterpret_cast<const uint64*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64);
		
		for(uint64 i = 0; i < count; ++i){
			if(pu[i] != pup[i % pattern_size]) return false;
		}
		return true;
	}
	
};


typedef DynamicPointer<Message>	MessagePointerT;

void receive_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr);
void complete_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr, ErrorConditionT const &_rerr);


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
	
	if(not static_cast<Message&>(*_rmsgptr).check()){
		THROW_EXCEPTION("Message check failed.");
	}
	size_t idx = static_cast<Message&>(*_rmsgptr).idx;
	cassert(not initarray[idx % initarraysize].cancel);
	
	++crtreadidx;
	
	if(crtreadidx == 1){
		idbg(crtreadidx<<" canceling all messages");
		for(auto msguid:message_uid_vec){
			idbg("Cancel message: "<<msguid);
			ctx.ipcmsgwriter->cancel(msguid, *ctx.ipcconfig, *ctx.ipctypemap, ipcconctx);
		}
	}else{
		idbg(crtreadidx);
	}
}

void complete_message(frame::ipc::ConnectionContext &_rctx, frame::ipc::MessagePointerT &_rmsgptr, ErrorConditionT const &_rerr){
	if(_rerr and _rerr != frame::ipc::error_message_canceled){
		THROW_EXCEPTION("Message complete with error");
	}
	size_t idx = static_cast<Message&>(*_rmsgptr).idx;
	cassert((!_rerr and not initarray[idx % initarraysize].cancel) or (initarray[idx % initarraysize].cancel and _rerr == frame::ipc::error_message_canceled));
	idbg(static_cast<Message&>(*_rmsgptr).str.size()<<' '<<_rerr.message());
}

}//namespace
 
int test_protocol_cancel(int argc, char **argv){
	
	Thread::init();
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("all");
	Debug::the().initStdErr(false, nullptr);
#endif
	
	for(int i = 0; i < 127; ++i){
		if(isprint(i) and !isblank(i)){
			pattern += static_cast<char>(i);
		}
	}
	
	size_t	sz = real_size(pattern.size());
	
	if(sz > pattern.size()){
		pattern.resize(sz - sizeof(uint64));
	}else if(sz < pattern.size()){
		pattern.resize(sz);
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
	
	ipcconfig.max_writer_multiplex_message_count = 6;
	
	frame::ipc::TestEntryway::initTypeMap<::Message>(ipctypemap, complete_message, receive_message);
	
	const size_t					start_count = 16;
	
	writecount = 16;//start_count;//
	
	for(; crtwriteidx < start_count; ++crtwriteidx){
		frame::ipc::MessageBundle	msgbundle;
		
		
		msgbundle.message_flags = initarray[crtwriteidx % initarraysize].flags;
		msgbundle.message_ptr = std::move(frame::ipc::MessagePointerT(new Message(crtwriteidx)));
		msgbundle.message_type_id = ctx.ipctypemap->index(msgbundle.message_ptr.get());
		//msgbundle.response_fnc = std::move(response_fnc);
		
		message_uid_vec.push_back(ipcmsgwriter.safeNewMessageUid(ipcconfig));
		
		ipcmsgwriter.enqueue(
			msgbundle,
			message_uid_vec.back(),
			ipcconfig, ipctypemap, ipcconctx
		);
		if(not initarray[crtwriteidx % initarraysize].cancel){
			idbg("do not cancel "<<message_uid_vec.back());
			message_uid_vec.pop_back(); //we do not cancel this one
		}
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
	
	bool is_running = true;
	
	while(is_running and !error){
		uint32 bufsz = ipcmsgwriter.write(buf, bufcp, false, ipcconfig, ipctypemap, ipcconctx, error);
		idbg("write "<<bufsz);
		if(not ipcmsgwriter.isNonSafeCacheEmpty()){
			ipcmsgwriter.safeMoveCacheToSafety();
		}
		if(!error and bufsz){
			frame::ipc::MessageReader::CompleteFunctionT	completefnc(std::cref(complete_lambda));
			ipcmsgreader.read(buf, bufsz, completefnc, ipcconfig, ipctypemap, ipcconctx, error);
			idbg("read");
		}else{
			idbg("done write");
			is_running = false;
		}
	}
	
	return 0;
}

