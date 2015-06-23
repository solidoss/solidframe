#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcserialization.hpp"

#include "frame/ipc/src/ipcmessagereader.hpp"
#include "frame/ipc/src/ipcmessagewriter.hpp"

using namespace solid;

int test_protocol(int argc, char **argv){
	frame::ipc::ConnectionContext	&ipcconctx(*(static_cast<frame::ipc::ConnectionContext*>(nullptr)));
	frame::ipc::AioSchedulerT		&ipcsched(*(static_cast<frame::ipc::AioSchedulerT*>(nullptr)));
	
	frame::ipc::Configuration		ipcconfig(ipcsched);
	frame::ipc::TypeIdMapT			ipctypemap;
	
	frame::ipc::MessageReader		ipcmsgreader;
	frame::ipc::MessageWriter		ipcmsgwriter;
	
	const uint16					bufcp[1024*4];
	char							buf[bufcp];
	
	ErrorConditionT					error;
	
	
	
	return 0;
}