// audit/src/log.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <bitset>
#include <vector>
#include <map>
#include <cstring>
#include "audit/log.hpp"
#include "system/common.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/timespec.hpp"
#include "utility/ostream.hpp"
#include "audit/log/logdata.hpp"
#include "system/debug.hpp"
#include "system/cstring.hpp"

#ifdef ON_SOLARIS
#include <strings.h>
#endif

using namespace std;

namespace solid{

#ifdef ON_WINDOWS
const unsigned fileoff =  (strlen(__FILE__) - strlen(strstr(__FILE__, "audit\\src")));
#else
const unsigned fileoff =  (strlen(__FILE__) - strlen(strstr(__FILE__, "audit/src")));
#endif

class stringoutbuf : public std::streambuf {
protected:
	string		s;
	//time_t		t;
	TimeSpec	ct;
	stringoutbuf& operator=(const stringoutbuf&);
	stringoutbuf(const stringoutbuf&);
public:
	// constructor
	stringoutbuf(){
		clear();
		//t = time(NULL);
		//ct.currentRealTime();
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
		s.append(_s, static_cast<size_t>(_sz));
		return _sz;
	}
};
//-----------------------------------------------------------------
struct StrLess{
	bool operator()(const char *_str1, const char *_str2)const{
		const int rv = cstring::casecmp(_str1, _str2);
		return rv < 0;
	}
};
struct ModuleStub{
	ModuleStub(const char *_name, uint32 _lvlmsk):name(_name), lvlmsk(_lvlmsk){}
	string	name;
	uint32	lvlmsk;
};

struct Log::Data: std::ostream{
	typedef std::bitset<LOG_BITSET_SIZE>			BitSetT;
	typedef std::vector<ModuleStub>					ModuleVectorT;
	typedef std::map<
		const char *, unsigned, StrLess>			StringMapT;

	Data():std::ostream(0), lvlmsk(0), pos(NULL){
		rdbuf(&outbuf);
		bs.reset();
		modvec.reserve(LOG_BITSET_SIZE);
	}
	~Data(){
		delete pos;
	}
	
	void setModuleMask(const char*);
	void setBit(const char *_pbeg, const char *_pend);
	void sendInfo();
	inline bool isActive()const{
		return pos && lvlmsk != 0 && bs.any();
	}
	unsigned registerModule(const char *_name, uint32 _lvlmsk = 0);
//data:
	uint32			lvlmsk;
	uint32			lvlmsk_set;
	OutputStream	*pos;
	stringoutbuf	outbuf;
	BitSetT			bs;
	ModuleVectorT	modvec;
	StringMapT		namemap;
	Mutex			m;
	string			procname;
};

/*static*/ const unsigned Log::any(Log::the().registerModule("ANY"));

//=====================================================================

namespace{
uint32 parseLevels(const char *_lvl){
	if(!_lvl) return 0;
	uint32 r = 0;
	
	while(*_lvl){
		switch(*_lvl){
			case 'i':
			case 'I':
				r |= Log::Info;
				break;
			case 'e':
			case 'E':
				r |= Log::Error;
				break;
			case 'w':
			case 'W':
				r |= Log::Warn;
				break;
			case 'v':
			case 'V':
				r |= Log::Verbose;
				break;
			case 'c':
				r |= Log::Input;
				break;
			case 'C':
				r |= Log::Output;
				break;
			default:
				break;
		}
		++_lvl;
	}
	return r;
}
}//namespace

void Log::Data::setBit(const char *_pbeg, const char *_pend){
	std::string str;
	str.assign(_pbeg, _pend - _pbeg);
	std::string	name;
	uint32		lvls = -1;
	{
		std::string	lvlstr;
		
		size_t		off = str.rfind(':');
		
		if(off == string::npos){
			name = str;
		}else{
			name = str.substr(0, off);
			lvlstr = str.substr(off, str.size() - off);
		}
		if(lvlstr.size()){
			lvls= parseLevels(lvlstr.c_str());
		}
	}
	
	if(!cstring::ncasecmp(name.c_str(), "all", name.size())){
		bs.set();
	}else if(!cstring::ncasecmp(name.c_str(), "none", name.size())){
		bs.reset();
	}else{
		unsigned pos = registerModule(name.c_str(), 0);
		modvec[pos].lvlmsk = lvls;
		bs.set(pos);
	}
}

unsigned Log::Data::registerModule(const char *_name, uint32 _lvlmsk){
	std::string name = _name;
	for(std::string::iterator it = name.begin(); it != name.end(); ++it){
		*it = toupper(*it);
	}
	
	StringMapT::const_iterator it = namemap.find(_name);
	
	if(it != namemap.end()){
		return it->second;
	}else{
		modvec.push_back(ModuleStub(name.c_str(), _lvlmsk));
		namemap[modvec.back().name.c_str()] = modvec.size() - 1;
		return modvec.size() - 1;
	}
}

void Log::Data::setModuleMask(const char *_opt){
	enum{
		SkipSpacesState,
		ParseNameState
	};
	bs.reset();
	if(_opt){
		const char *pbeg = _opt;
		const char *pcrt = _opt;
		int state = 0;
		while(*pcrt){
			if(state == SkipSpacesState){
				if(isspace(*pcrt)){
					++pcrt;
					pbeg = pcrt;
				}else{
					++pcrt;
					state = ParseNameState; 
				}
			}else if(state == ParseNameState){
				if(!isspace(*pcrt)){
					++pcrt;
				}else{
					setBit(pbeg, pcrt);
					pbeg = pcrt;
					state = 0;
				}
			}
		}
		if(pcrt != pbeg){
			setBit(pbeg, pcrt);
		}
	}
}

void Log::Data::sendInfo(){
	if(!pos || !isActive()) return;
	//we send - version, the process id, the process name, the list of modules
	audit::LogHead lh(Thread::processId(), procname.size(), modvec.size());
	lh.convertToNetwork();
	pos->write((const char*) &lh, sizeof(lh));
	//now write the procname
	pos->write(procname.data(), procname.size());
	for(ModuleVectorT::const_iterator it(modvec.begin()); it != modvec.end(); ++it){
		uint32 hsz = it->name.size();
		uint32 sz = toNetwork(hsz);
		pos->write((const char*)&sz, 4);
		pos->write(it->name.c_str(), hsz);
	}
}
//=====================================================================
#ifdef HAS_SAFE_STATIC
/*static*/ Log& Log::the(){
	static Log l;
	return l;
}
#else
/*static*/ Log& Log::log_instance(){
	static Log l;
	return l;
}
/*static*/void Log::once_log(){
	log_instance();
}

/*static*/ Log& Log::the(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_log, once);
	return log_instance();
}

#endif

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
	uint32					filenamelen = strlen(_file) + 1;//including the terminal \0
	uint32					functionnamelen = strlen(_function) + 1;//including the terminal \0
	audit::LogRecordHead	&lh(*((audit::LogRecordHead*)s.data()));
	TimeSpec				tt;
	tt.currentRealTime();
	//tt -= ct;
	lh.set(_level, _module, _id, _line, static_cast<uint32>(tt.seconds()), tt.nanoSeconds());
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

void Log::levelMask(const char *_msk){
	Locker<Mutex> lock(d.m);
	if(!_msk){
		_msk = "iewvcd";
	}
	d.lvlmsk = parseLevels(_msk);
}
void Log::moduleMask(const char *_msk){
	Locker<Mutex>	lock(d.m);
	if(!_msk){
		_msk = "all";
	}
	d.setModuleMask(_msk);
}

bool Log::reinit(const char* _procname, OutputStream *_pos, const char *_modmsk, const char *_lvlmsk){
	Locker<Mutex>	lock(d.m);
	delete d.pos;
	d.pos = _pos;
	if(!_lvlmsk){
		_lvlmsk = "iewvcCd";
	}
	uint32			lvlmsk = parseLevels(_lvlmsk);
	d.lvlmsk = lvlmsk;
	d.setModuleMask(_modmsk);
	d.procname = _procname;
	d.sendInfo();
	return d.isActive();
}

void Log::reinit(OutputStream *_pos){
	Locker<Mutex>	lock(d.m);
	delete d.pos;
	d.pos = _pos;
	d.sendInfo();
}

void Log::moduleNames(std::string &_ros){
	Locker<Mutex>	lock(d.m);
	for(Data::ModuleVectorT::const_iterator it(d.modvec.begin()); it != d.modvec.end(); ++it){
		_ros += it->name;
		_ros += ' ';
	}
}
void Log::setAllModuleBits(){
	Locker<Mutex>	lock(d.m);
	d.bs.set();
}
void Log::resetAllModuleBits(){
	Locker<Mutex>	lock(d.m);
	d.bs.reset();
}
void Log::setModuleBit(unsigned _v){
	Locker<Mutex>	lock(d.m);
	d.bs.set(_v);
}
void Log::resetModuleBit(unsigned _v){
	Locker<Mutex>	lock(d.m);
	d.bs.reset(_v);
}
unsigned Log::registerModule(const char *_name){
	Locker<Mutex>	lock(d.m);
	return d.registerModule(_name);
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
bool Log::isSet(unsigned _v, unsigned _lvl)const{
	return (d.lvlmsk & _lvl) && _v < d.bs.size() && d.bs[_v] && d.modvec[_v].lvlmsk & _lvl;
}

}//namespace solid