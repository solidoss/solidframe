#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/socketdevice.hpp"
#include "system/socketaddress.hpp"
#include "system/condition.hpp"
#include "system/debug.hpp"

#include "utility/istream.hpp"

#include "audit/log/logconnector.hpp"
#include "audit/log/logclientdata.hpp"
#include "audit/log/logrecord.hpp"
#include "audit/log/logrecorder.hpp"
#include "audit/log/logmanager.hpp"
#include "utility/stack.hpp"
#include <vector>
#include <string>

using namespace std;

namespace audit{

struct SocketInputStream: InputStream{
	SocketInputStream(SocketDevice &_rsd):sd(_rsd){}
	int read(char *_pb, uint32 _bl, uint32){
		return sd.read(_pb, _bl);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
	void close(){
		sd.shutdownReadWrite();
		sd.close();
	}
	SocketDevice	sd;
};

struct LogManager::Data{
	struct Channel{
		Channel(InputStream *_pins):pins(_pins), uid(0){}
		InputStream		*pins;
		uint32		uid;
	};
	struct Listener{
		Listener(): ready(true), uid(0){}
		SocketDevice	sd;
		bool			ready;
		uint32 			uid;
	};
	typedef std::pair<LogConnector*, uint32> 	ConnectorPairT;
	typedef std::vector<ConnectorPairT>			ConnectorVectorT;
	typedef std::vector<Channel>				ChannelVectorT;
	typedef std::vector<Listener>				ListenerVectorT;
	typedef Stack<uint32>						PosStackT;
	Data();
	~Data();
	enum State{
		Stopped,
		Running,
		Stopping,
	};
//data:
	Mutex					m;
	Condition				statecnd;
	State					state;
	ConnectorVectorT		conv;
	PosStackT				cons;
	ListenerVectorT			lsnv;
	PosStackT				lsns;
	ChannelVectorT			chnv;
	PosStackT				chns;
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
	lsnv.reserve(64);
}
LogManager::Data::~Data(){
	//erase all the connectors
	for(ConnectorVectorT::iterator it(conv.begin()); it != conv.end(); ++it){
		delete it->first;
	}
}

//============================================================
LogManager::LogManager():d(*(new Data)){
	
}

LogManager::~LogManager(){
	stop(true);
	delete &d;
}

LogManager::UidT LogManager::insertChannel(InputStream *_pins){
	Locker<Mutex> lock(d.m);
	if(d.state != Data::Running){return UidT(0xffffffff, 0xffffffff);}
	UidT	uid;
	if(d.chns.size()){
		d.chnv[d.chns.top()].pins = _pins;
		uid.first = d.chns.top();d.chns.pop();
		uid.second = d.chnv[uid.first].uid;
	}else{
		d.chnv.push_back(Data::Channel(_pins));
		uid.first = d.chnv.size() - 1;
		uid.second = 0;
	}
	ChannelWorker *pw = new ChannelWorker(*this, uid.first);
	pw->start(false, true, 100000);
	return uid;
}
LogManager::UidT LogManager::insertListener(const char *_addr, const char *_port){
	Locker<Mutex> lock(d.m);
	if(d.state != Data::Running){return UidT(0xffffffff, 0xffffffff);}
	UidT	uid;
	if(d.lsns.size()){
		uid.first = d.lsns.top();d.lsns.pop();
		uid.second = d.lsnv[uid.first].uid;
	}else{
		if(d.lsnv.size() == 63){
			return UidT(0xffffffff, 0xffffffff);
		}
		d.lsnv.push_back(Data::Listener());
		uid.first = d.lsnv.size() - 1;
		uid.second = 0;
	}
	ListenerWorker *pw = new ListenerWorker(*this, _addr, _port, uid.first);
	pw->start(false, true, 100000);
	return uid;
}

void LogManager::eraseClient(const LogManager::UidT &_ruid){
	Locker<Mutex> lock(d.m);
	if(_ruid.first < d.chnv.size() && _ruid.second != d.chnv[_ruid.first].uid){
		d.chnv[_ruid.first].pins->close();
	}
}
void LogManager::eraseListener(const LogManager::UidT &_ruid){
	Locker<Mutex> lock(d.m);
	if(_ruid.first < d.lsnv.size() && _ruid.second != d.lsnv[_ruid.first].uid){
		d.lsnv[_ruid.first].sd.close();
		d.lsnv[_ruid.first].ready = false;
	}
}

int LogManager::start(){
	if(d.state == Data::Running) return OK;
	if(d.state != Data::Stopped){
		stop(true);
	}
	d.state = Data::Running;
	return OK;
}
void LogManager::stop(bool _wait){
	Locker<Mutex> lock(d.m);
	if(d.state == Data::Running){
		d.state = Data::Stopping;
		for(Data::ChannelVectorT::const_iterator it(d.chnv.begin()); it != d.chnv.end(); ++it){
			if(it->pins){
				it->pins->close();
			}
		}
		for(Data::ListenerVectorT::iterator it(d.lsnv.begin()); it != d.lsnv.end(); ++it){
			it->sd.cancel();
			it->sd.shutdownReadWrite();
			it->sd.close();
			it->ready = false;
		}
	}
	if(_wait){
		while(d.lsnv.size() != d.lsns.size() || d.chnv.size() != d.chns.size()){
			d.statecnd.wait(lock);
		}
		d.state = Data::Stopped;
	}
}
LogManager::UidT LogManager::insertConnector(LogConnector *_plc){
	Locker<Mutex> lock(d.m);
	UidT	uid;
	if(d.cons.size()){
		d.conv[d.cons.top()].first = _plc;
		uid.first = d.cons.top();
		uid.second = d.conv[d.cons.top()].second;
	}else{
		d.conv.push_back(Data::ConnectorPairT(_plc, 0));
		uid.first = d.conv.size() - 1;
		uid.second = 0;
	}
	return uid;
}
void LogManager::eraseConnector(const UidT &_ruid){
	Locker<Mutex> lock(d.m);
	if(_ruid.first < d.conv.size() && _ruid.second != d.conv[_ruid.first].second){
		if(d.conv[_ruid.first].first && d.conv[_ruid.first].first->destroy()){
			delete d.conv[_ruid.first].first;
			d.conv[_ruid.first].first = NULL;
		}
	}
}
void LogManager::runListener(ListenerWorker &_w){
	//first we need to create the socket device:
	SocketDevice sd;
	{
		ResolveData rd =  synchronous_resolve(_w.addr.c_str(), _w.port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
		if(!rd.empty()){
			sd.create(rd.begin());
			sd.prepareAccept(rd.begin(), 10);
		}else{
			edbgx(Dbg::log, "create address "<<_w.addr<<'.'<<_w.port);
		}
	}
	if(sd.ok()){
		d.m.lock();
		if(d.lsnv[_w.idx].ready){
			d.lsnv[_w.idx].sd = sd;
		}
		d.m.unlock();
		SocketDevice &rsd(d.lsnv[_w.idx].sd);
		SocketDevice csd;
		while(rsd.accept(csd) == OK){
			SocketInputStream *pis = new SocketInputStream(csd);
			if(this->insertChannel(pis).first == 0xffffffff){
				delete pis;
			}
		}
		rsd.close();
	}
	//in the end we unregister the listener
	Locker<Mutex> lock(d.m);
	++d.lsnv[_w.idx].uid;
	d.lsns.push(_w.idx);
	d.statecnd.signal();
}

void readClientData(LogClientData &_rcd, InputStream &_ris){
	if(!_ris.readAll((char*) &_rcd.head, sizeof(_rcd.head))) return;
	_rcd.head.convertToHost();
	_rcd.procname.resize(_rcd.head.procnamelen);
	if(!_ris.readAll((char*) _rcd.procname.data(), _rcd.head.procnamelen)) return;
	for(uint i = 0; i < _rcd.head.modulecnt; ++i){
		_rcd.modulenamev.push_back(string());
		uint32 	sz;
		if(!_ris.readAll((char*)&sz, sizeof(uint32))) return;
		sz = toHost(sz);
		_rcd.modulenamev.back().resize(sz);
		if(!_ris.readAll((char*)_rcd.modulenamev.back().data(), sz)) return;
	}
}

void LogManager::runChannel(ChannelWorker &_w){
	InputStream *pis = NULL;
	LogClientData	cd;
	cd.idx = _w.idx;
	{
		Locker<Mutex> lock(d.m);
		pis = d.chnv[_w.idx].pins;
		cd.uid = d.chnv[_w.idx].uid;
	}
	if(pis){
		//first read clientdata
		readClientData(cd, *pis);
		//then wait for records:
		typedef std::vector<uint32> IndexVectorT;
		LogRecorderVector	rv;
		IndexVectorT		indexv;
		LogRecord			rec;
		//uint32 sz;
		while(
			pis->readAll((char*)&rec.head, sizeof(rec.head))
		){
			rec.head.convertToHost();
			if(!pis->readAll(rec.data(rec.size()), rec.size(rec.head.datalen))) break;
			//idbg("read: "<<rec.data());
			//prepare repositories, fetching the list of writers
			d.m.lock();
			for(Data::ConnectorVectorT::const_iterator it(d.conv.begin()); it != d.conv.end(); ++it){
				if(it->first && it->first->receivePrepare(rv, rec, cd)){
					indexv.push_back(it - d.conv.begin());
				}
			}
			d.m.unlock();
			//write the record to the writers:
			for(LogRecorderVector::const_iterator it(rv.begin()); it != rv.end(); ++it){
				(*it)->record(cd, rec);
			}
			rv.clear();
			Locker<Mutex> lock(d.m);
			//done with the repositories:
			for(IndexVectorT::const_iterator it(indexv.begin()); it != indexv.end(); ++it){
				if(d.conv[*it].first->receiveDone()){
					//not in use - its safe to delete it right now
					delete d.conv[*it].first;
					d.conv[*it].first = NULL;
					++d.conv[*it].second;
					d.cons.push(*it);
				}
			}
			indexv.clear();
		}
	}
	Locker<Mutex> lock(d.m);
	for(Data::ConnectorVectorT::const_iterator it(d.conv.begin()); it != d.conv.end(); ++it){
		if(it->first){
			it->first->eraseClient(cd.idx, cd.uid);
		}
	}
	delete pis;
	d.chnv[_w.idx].pins = NULL;
	++d.chnv[_w.idx].uid;
	d.chns.push(_w.idx);
	d.statecnd.signal();
}
//==============================================================
char* LogRecord::data(uint32 _sz){
	if(_sz < cp) return d;
	char *tmp = new char[_sz + 16];
	delete []d;
	d = tmp;
	return d;
}
}//namespace audit

