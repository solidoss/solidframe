#include "audit/log/logmanager.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"

using namespace std;

namespace audit{

struct SockIStream: IStream{
	int read(char *_pb, uint32 _bl, uint32){
		return sd.read(_pb, _bl);
	}
	SocketDevice	sd;
};

struct LogManager::Data{
	struct Channel{
		Channel(IStream *_pins):pins(_pins), uid(0){}
		IStream		*pins;
		uint32		uid;
	};
	struct Listener{
		Listener(): uid(0){}
		SocketDevice	sd;
		uint32 			uid;
	};
	typedef std::pair<Connector*, uint32> 		ConnectorPairTp;
	typedef std::vector<ConnectorPairTp>		ConnectorVectorTp;
	typedef std::vector<Channel>				ChannelVectorTp;
	typedef std::vector<Listener>				ListenerVectorTp;
	typedef std::stack<uint32>					PosStackTp;
	Data();
	~Data();
	enum State{
		Stopped,
		Started,
		Stopping,
	};
//data:
	Mutex					m;
	Condition				statecnd;
	State					state;
	ConnectorVectorTp		conv;
	PosStackTp				cons;
	ListenerVectorTp		lsnv;
	PosStackTp				lsns;
	ChannelVectorTp			chnv;
	PosStackTp				chns;
};

struct LogManager::ListenerWorker: Thread{
	ListenerWorker(
		LogManager &_rlm,
		const char *_addr,
		const char *_port,
		uint32	_idx
	):lm(_rlm),addr(_addr), port(_port), idx(_idx){}
	void run(){
		lm.runListener(*this);
	}
	LogManager	&lm;
	string		addr;
	string		port;
	uint32		idx;
};

struct LogManager::ChannelWorker: Thread{
	ChannelWorker(
		LogManager &_rlm,
		uint32	_idx
	):lm(_rlm), idx(_idx){}
	void run(){
		lm.runChannel(*this);
	}
	LogManager	&lm;
	uint32		idx;
};

LogManager::Data::Data():state(Stopped){
}
LogManager::Data::~Data(){
	//TODO
}

//============================================================
LogManager::LogManager():d(*(new Data)){}

LogManager::~LogManager(){
	stop(true);
	delete &d;
}

LogManager::UidTp LogManager::addChannel(IStream *_pins){
	Mutex::Locker lock(d.m);
	UidTp	uid;
	if(d.chns.size()){
		d.chnv[d.chns.top()].pins = _pins;
		uid.first = d.chns.top();
		uid.second = d.chnv[d.chns.top()].second;
	}else{
		d.chnv.push_back(Channel(_pins));
		uid.first = d.chnv.size() - 1;
		uid.second = 0;
	}
	ChannelWorker *pw = new ChannelWorker(*this, uid.first);
	pw->start();
	return uid;
}
LogManager::UidTp LogManager::addListener(const char *_addr, const char *_port){
}

void LogManager::eraseClient(const LogManager::UidTp &_ruid){
}
void LogManager::eraseListener(const LogManager::UidTp &_ruid){
}

int LogManager::start(){
}
void LogManager::stop(bool _wait){
}
LogManager::UidTp LogManager::insertConnector(LogConnector *_plc){
}
void LogManager::eraseConnector(const UidTp &_ruid){
}
void LogManager::runListener(ListenerWorker &_w){
}
void LogManager::runChannel(ChannelWorker &_w){
}

}//namespace audit

