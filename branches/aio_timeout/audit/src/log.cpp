#include <bitset>
#include <vector>
#include <cstring>
#include "audit/log.hpp"
#include "system/common.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"
#include "utility/ostream.hpp"
#include "audit/log/logdata.hpp"
#include "system/debug.hpp"

#ifdef ON_SUN
#include <strings.h>
#endif

using namespace std;
const unsigned fileoff = (strstr(__FILE__, "audit/src") - __FILE__);

class stringoutbuf : public std::streambuf {
protected:
	string		s;
	time_t		t;
	TimeSpec	ct;
	stringoutbuf& operator=(const stringoutbuf&);
	stringoutbuf(const stringoutbuf&);
public:
	// constructor
	stringoutbuf(){
		clear();
		t = time(NULL);
		clock_gettime(CLOCK_MONOTONIC, &ct);
	}
	
	const char* data()const{
		return s.data();
	}
	
	uint32	size()const{return s.size();}
	
	void clear(){
		s.resize(sizeof(audit::LogRecordHead));
	}
	
	void current(
		uint16 _level,
		uint16 _module,
		uint32 _id,
		uint32 _line,
		const char *_file,
		const char *_function
	);
	
	void setLength(){
		audit::LogRecordHead &lh(*((audit::LogRecordHead*)s.data()));
		lh.dataLength(s.size() - lh.datalen);
	}
protected:
	// write one character
	/*virtual*/ int_type overflow(int_type c){
		s += (char)c;
		return c;
	}
	// write multiple characters
	/*virtual*/	std::streamsize xsputn(
		const char* _s,
		std::streamsize _sz
	){
		s.append(_s, _sz);
		return _sz;
	}
};

struct Log::Data: std::ostream{
	typedef std::bitset<LOG_BITSET_SIZE>	BitSetTp;
	typedef std::vector<const char*>		NameVectorTp;

	Data():std::ostream(0), lvlmsk(0), pos(NULL){
		rdbuf(&outbuf);
	}
	~Data(){
		delete pos;
	}
	
	void setModuleBit(const char *_pbeg, const char *_pend);
	void parseModuleOptions(const char *_modopt);
	void sendInfo();
	inline bool isActive()const{
		return lvlmsk != 0 && bs.any();
	}
//data:
	uint32			lvlmsk;
	uint32			lvlmsk_set;
	OStream			*pos;
	stringoutbuf	outbuf;
	BitSetTp		bs;
	NameVectorTp	nv;
	Mutex			m;
	string			procname;
};

/*static*/ const unsigned Log::any(Log::instance().registerModule("ANY"));

//=====================================================================
void Log::Data::setModuleBit(const char *_pbeg, const char *_pend){
	if(!strncasecmp(_pbeg, "NONE", _pend - _pbeg)){
		bs.reset();
	}else if(!strncasecmp(_pbeg, "ALL", _pend - _pbeg)){
		bs.set();
	}else for(NameVectorTp::const_iterator it(nv.begin()); it != nv.end(); ++it){
		if(!strncasecmp(_pbeg, *it, _pend - _pbeg) && (int)strlen(*it) == (_pend - _pbeg)){
			bs.set(it - nv.begin());
		}
	}
}

void Log::Data::parseModuleOptions(const char *_modopt){
	if(_modopt){
		const char *pbeg = _modopt;
		const char *pcrt = _modopt;
		int state = 0;
		while(*pcrt){
			if(state == 0){
				if(isspace(*pcrt)){
					++pcrt;
					pbeg = pcrt;
				}else{
					++pcrt;
					state = 1; 
				}
			}else if(state == 1){
				if(!isspace(*pcrt)){
					++pcrt;
				}else{
					setModuleBit(pbeg, pcrt);
					pbeg = pcrt;
					state = 0;
				}
			}
		}
		if(pcrt != pbeg){
			setModuleBit(pbeg, pcrt);
		}
	}
}
void Log::Data::sendInfo(){
	if(!pos || !isActive()) return;
	//we send - version, the process id, the process name, the list of modules
	audit::LogHead lh(Thread::processId(), procname.size(), nv.size());
	lh.convertToNetwork();
	pos->write((const char*) &lh, sizeof(lh));
	//now write the procname
	pos->write(procname.data(), procname.size());
	for(NameVectorTp::const_iterator it(nv.begin()); it != nv.end(); ++it){
		uint32 hsz = strlen(*it);
		uint32 sz = toNetwork(hsz);
		pos->write((const char*)&sz, 4);
		pos->write(*it, hsz);
	}
}
//=====================================================================
/*static*/ Log& Log::instance(){
	//TODO: staticproblem
	static Log l;
	return l;
}

void stringoutbuf::current(
	uint16 _level,
	uint16 _module,
	uint32 _id,
	uint32 _line,
	const char *_file,
	const char *_function
){
	_file += fileoff;
	clear();
	audit::LogRecordHead &lh(*((audit::LogRecordHead*)s.data()));
	uint32	filenamelen = strlen(_file) + 1;//including the terminal \0
	uint32	functionnamelen = strlen(_function) + 1;//including the terminal \0
	TimeSpec tt;
	clock_gettime(CLOCK_MONOTONIC, &tt);
	tt -= ct;
	lh.set(_level, _module, _id, _line, t + tt.seconds(), tt.nanoSeconds());
	idbgx(Dbg::log, "time ("<<(t + tt.seconds())<<','<<tt.nanoSeconds()<<')');
	lh.set(filenamelen, functionnamelen);
	//no function so we can overpass the bitorder conversion for datalen
	lh.datalen = sizeof(audit::LogRecordHead);
	s.append(_file, filenamelen);//taking the 0 too
	s.append(_function, functionnamelen);//taking the 0 too
}

Log::Log():d(*(new Data)){
}
Log::~Log(){
	delete &d;
}

bool Log::reinit(const char* _procname, uint32 _lvlmsk, const char *_modopt, OStream *_pos){
	Mutex::Locker lock(d.m);
	delete d.pos;
	d.pos = _pos;
	d.lvlmsk_set = _lvlmsk;
	//this is to ensure we have only to check the level mask and not the outstream too
	if(d.pos) d.lvlmsk = _lvlmsk;
	else d.lvlmsk = 0;
	d.parseModuleOptions(_modopt);
	d.procname = _procname;
	d.sendInfo();
	return d.isActive();
}

void Log::reinit(OStream *_pos){
	Mutex::Locker lock(d.m);
	delete d.pos;
	d.pos = _pos;
	if(d.pos) d.lvlmsk = d.lvlmsk_set;
	else d.lvlmsk = 0;
	d.sendInfo();
}

void Log::moduleNames(std::string &_ros){
	Mutex::Locker lock(d.m);
	for(Data::NameVectorTp::const_iterator it(d.nv.begin()); it != d.nv.end(); ++it){
		_ros += *it;
		_ros += ' ';
	}
}
void Log::setAllModuleBits(){
	Mutex::Locker lock(d.m);
	d.bs.set();
}
void Log::resetAllModuleBits(){
	Mutex::Locker lock(d.m);
	d.bs.reset();
}
void Log::setModuleBit(unsigned _v){
	Mutex::Locker lock(d.m);
	d.bs.set(_v);
}
void Log::resetModuleBit(unsigned _v){
	Mutex::Locker lock(d.m);
	d.bs.reset(_v);
}
unsigned Log::registerModule(const char *_name){
	Mutex::Locker lock(d.m);
	d.nv.push_back(_name);
	return d.nv.size() - 1;
}

std::ostream& Log::record(
	Level _lvl,
	unsigned _module,
	uint32	_id,
	const char *_file,
	const char *_fnc,
	int _line
){
	d.m.lock();
	d.outbuf.current(_lvl, _module, _id, _line, _file, _fnc);
	return d;
}
void Log::done(){
	d.outbuf.setLength();
	d.pos->write(d.outbuf.data(), d.outbuf.size());
	d.m.unlock();
}
bool Log::isSet(unsigned _mod, unsigned _level)const{
	return (d.lvlmsk & _level) && d.bs[_mod];
}

