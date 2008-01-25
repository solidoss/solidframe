#include "clientserver/core/commandexecuter.h"
#include "clientserver/core/command.h"
#include "clientserver/core/server.h"
#include <deque>
#include <cassert>
#include "system/mutex.h"
#include "system/debug.h"
#include "utility/stack.h"
#include "utility/queue.h"


namespace clientserver{


struct CommandExecuter::Data{
	Data():pm(NULL), extsz(0), sz(0){}
	enum{
		Running, Stopping
	};
	void push(CmdPtr<Command> &_cmd);
	typedef std::pair<CmdPtr<Command>, uint32>	CommandPairTp;
	typedef std::deque<CommandPairTp >			CommandDequeTp;
	typedef std::deque<CmdPtr<Command> >		CommandExtDequeTp;
	typedef Queue<uint32>						ExecQueueTp;
	typedef Stack<uint32>						FreeStackTp;
	Mutex 				*pm;
	uint32				extsz;
	uint32				sz;
	CommandDequeTp		cdq;
	CommandExtDequeTp	cedq;
	FreeStackTp			fs;
	FreeStackTp			fs2;
	ExecQueueTp			sq;
	ExecQueueTp			eq;
};

void CommandExecuter::Data::push(CmdPtr<Command> &_cmd){
	if(fs.size()){
		cdq[fs.top()].first = _cmd;
		sq.push(fs.top());
		fs.pop();
	}else{
		cedq.push_back(_cmd);
		++extsz;
		sq.push(cdq.size() + extsz - 1);
	}
}

CommandExecuter::CommandExecuter():d(*(new Data)){
	state(Data::Running);
}

CommandExecuter::~CommandExecuter(){
	delete &d;
}

int CommandExecuter::signal(CmdPtr<Command> &_cmd){
	assert(d.pm->locked());
	if(this->state() != Data::Running){
		_cmd.clear();
		return 0;//no reason to raise the pool thread!!
	}
	d.push(_cmd);
	return Object::signal(S_CMD | S_RAISE);
}

int CommandExecuter::execute(ulong _evs, TimeSpec &_rtout){
	d.pm->lock();
	if(d.extsz){
		for(Data::CommandExtDequeTp::const_iterator it(d.cedq.begin()); it != d.cedq.end(); ++it){
			d.cdq.push_back(Data::CommandPairTp(*it, 0));
		}
		d.sz += d.cedq.size();
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
				d.pm->unlock();
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
	d.pm->unlock();
	if(state() == Data::Running){
		while(d.eq.size()){
			uint32 pos = d.eq.front();
			d.eq.pop();
			Data::CommandPairTp &rcp(d.cdq[d.eq.front()]);
			switch(rcp.first->execute(*this, CommandUidTp(d.eq.front(), rcp.second))){
				case BAD: ++rcp.second;rcp.first.clear(); d.fs2.push(pos);break;
				case OK: d.eq.push(pos); break;
				case NOK:break;
			}
		}
	}else{
		for(Data::CommandDequeTp::iterator it(d.cdq.begin()); it != d.cdq.end(); ++it){
			if(it->first.ptr()){
				delete it->first.release();
			}
		}
		state(-1);
		return BAD;
	}
	if(d.fs2.size()){
		d.pm->lock();
		d.sz -= d.fs2.size();
		while(d.fs2.size()){
			d.fs.push(d.fs2.top());
			d.fs2.pop();
		}
		d.pm->unlock();
	}
	return NOK;
}

void CommandExecuter::mutex(Mutex *_pmut){
	d.pm = _pmut;
}

int CommandExecuter::execute(){
	assert(false);
}

void CommandExecuter::receiveIStream(
	StreamPtr<IStream> &_sp,
	const FileUidTp	& _fuid,
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveIStream(_sp, _fuid, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}
void CommandExecuter::receiveOStream(
	StreamPtr<OStream> &_sp,
	const FileUidTp	&_fuid,
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveOStream(_sp, _fuid, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}
void CommandExecuter::receiveIOStream(
	StreamPtr<IOStream> &_sp,
	const FileUidTp	&_fuid,
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveIOStream(_sp, _fuid, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}
void CommandExecuter::receiveString(
	const std::string &_str,
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveString(_str, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}
void CommandExecuter::receiveNumber(
	const int64 &_no,
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveNumber(_no, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}
void CommandExecuter::receiveError(
	int _errid, 
	const RequestUidTp &_requid,
	const ObjectUidTp&_from,
	const clientserver::ipc::ConnectorUid *_conid
){
	if(d.cdq[_requid.first].second == _requid.second){
		if(d.cdq[_requid.first].first->receiveError(_errid, _from, _conid) == OK){
			d.eq.push(_requid.first);
		}
	}
}


}//namespace clientserver



