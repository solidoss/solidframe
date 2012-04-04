#include "audit/log/logconnectors.hpp"
#include "audit/log/logclientdata.hpp"
#include "audit/log/logrecorders.hpp"
#include "system/cassert.hpp"
#include <vector>
#include <string>

#ifdef ON_WINDOWS
#include <time.h>
#endif

using namespace std;

namespace audit{

LogConnector::LogConnector():usecnt(0), mustdie(false){
}
/*virtual*/ LogConnector::~LogConnector(){
}
bool LogConnector::destroy(){
	mustdie = true;
	return usecnt == 0;
}
bool LogConnector::receivePrepare(
	LogRecorderVector& _outrv,
	const LogRecord &_rrec,
	const LogClientData &_rcl
){
	if(mustdie) return false;
	uint32 sz = _outrv.size();
	doReceive(_outrv, _rrec, _rcl);
	if(sz != _outrv.size()){
		++usecnt;
		return true;
	}
	return false;
}
bool LogConnector::receiveDone(){
	--usecnt;
	if(mustdie && !usecnt) return true;
	return false;
}

//-------------------------------------------------------------------------
struct LogBasicConnector::Data{
	struct RecorderVector: std::vector<LogRecorder*>{
		~RecorderVector();
	};
	std::string			prfx;
	RecorderVector	rv;
};

LogBasicConnector::Data::RecorderVector::~RecorderVector(){
	for(const_iterator it(begin()); it != end(); ++it){
		delete *it;
	}
}
//-------------------------------------------------------------------------
LogBasicConnector::LogBasicConnector(const char *_prfx):d(*(new Data)) {
	prefix(_prfx);
}
void LogBasicConnector::prefix(const char *_prfx){
	if(_prfx){
		d.prfx = _prfx;
		if(d.prfx.size() && d.prfx[d.prfx.size() - 1] != '/'){
			d.prfx += '/';
		}
	}else d.prfx.clear();
}
LogBasicConnector::~LogBasicConnector(){
	delete &d;
}
/*virtual*/ void LogBasicConnector::eraseClient(uint32 _idx, uint32 _uid){
	if(_idx < d.rv.size()){
		//cassert(d.rv[_idx]);
		delete d.rv[_idx];
		d.rv[_idx] = NULL;
	}
}
/*virtual*/ void LogBasicConnector::doReceive(
	LogRecorderVector& _outrv,
	const LogRecord &_rrec,
	const LogClientData &_rcl
){	
	if(_rcl.idx >= d.rv.size()){
		//TODO ensure there a zeros filling 
		d.rv.resize(_rcl.idx + 1);
	}
	LogRecorder *plr = d.rv[_rcl.idx];
	if(!plr){	
		plr = d.rv[_rcl.idx] = createRecorder(_rcl);
	}
	_outrv.push_back(plr);
}
LogRecorder* LogBasicConnector::createRecorder(const LogClientData &_rcl){
	string	pth(d.prfx);
	pth += _rcl.procname;
	char	buf[128];
	time_t	t = time(NULL);
	tm		*ploctm;
#ifdef ON_WINDOWS
	ploctm = localtime(&t);
#else
	tm		loctm;
	ploctm = localtime_r(&t, &loctm);
#endif
	sprintf(
		buf,
		"_%04u-%02u-%02u__%02u_%02u_%02u__%06u",
		ploctm->tm_year + 1900,
		ploctm->tm_mon + 1, 
		ploctm->tm_mday,
		ploctm->tm_hour,
		ploctm->tm_min,
		ploctm->tm_sec,
		_rcl.head.procid
	);
	pth += buf;
	LogFileRecorder* pl = new LogFileRecorder(pth.c_str());
	pl->open();
	return pl;
}

}//namespace audit
