#include "clientserver/commandexecuter.h"
#include "clientserver/command.h"
#include <deque>
#include <cassert>
#include "system/mutex.h"
#include "utility/stack.h"
#include "utility/queue.h"


namespace clientserver{


struct CommandExecuter::Data{
	Data():pm(NULL), extsz(0){}
	void push(CmdPtr<Command> &_cmd);
	typedef std::pair<CmdPtr<Command>, uint32>	CommandPairTp;
	typedef std::deque<CommandPairTp >			CommandDequeTp;
	typedef std::deque<CmdPtr<Command> >		CommandExtDequeTp;
	typedef Queue<uint32>						ExecQueueTp;
	typedef Stack<uint32>						FreeStackTp;
	Mutex 				*pm;
	uint32				extsz;
	CommandDequeTp		cdq;
	CommandExtDequeTp	cedq;
	FreeStackTp			fs;
	ExecQueueTp			sq;
	ExecQueueTp			eq;
};

void CommandExecuter::Data::push(CmdPtr<Command> &_cmd){
	if(fs.size()){
		cdq[fs.top()] = _cmd;
		sq.push(fs.top());
		fs.pop();
	}else{
		cedq.push_back(CommandPairTp(_cmd));
		++extsz;
		sq.push(cdq.size() + extsz - 1);
	}
}

CommandExecuter::CommandExecuter(){
	state(Data::Running);
}

CommandExecuter::~CommandExecuter(){
}

int CommandExecuter::signal(CmdPtr<Command> &_cmd){
	assert(pm->locked());
	if(this->state() != Data::Running){
		_cmd.clear();
		return 0;//no reason to raise the pool thread!!
	}
	d.push(_cmd);
	return signal(S_CMD | S_RAISE);
}

int CommandExecuter::execute(ulong _evs, TimeSpec &_rtout){
	d.mut->lock();
	if(d.extsz){
		for(CommandExtDequeTp::const_iterator it(d.cedq.begin()); it != d.cedq.end(); ++it){
			d.cdq.push_back(Data::CommandPairTp(*it, 0));
		}
		d.cedq.clear();
		d.extsz = 0;
	}
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbg("signalmask "<<sm);
		if(sm & S_KILL){
			state(Data::Stopping);
			if(!d.sz){//no command
				state(-1);
				d.mut->unlock();
				Server::the().removeFileManager();
				return BAD;
			}
		}
		if(sm & S_CMD){
			while(d.sq.size()){
				d.eq.push(d.sq.front());
				d.sq.pop();
			}
		}
	}
	d.mut->unlock();
	
	while(d.eq.size()){
		Data::CommandPairTp &rcp(d.cdq[d.eq.front()]);
		if(rcp.first->execute(*this, CommandUidTp(d.eq.front(), rcp.second));
		d.eq.pop();
	}
}

void CommandExecuter::mutex(Mutex *_pmut){
	d.pm = _pmut;
}

int CommandExecuter::execute(){
	assert(false);
}



}//namespace clientserver


