// system/src/debug.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#define DO_EXPORT_DLL 1
#ifdef ON_WINDOWS
#include <process.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <bitset>
#endif

#include "system/timespec.hpp"
#include "system/socketdevice.hpp"
#include "system/filedevice.hpp"
#include "system/cassert.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "system/directory.hpp"
#include "system/cstring.hpp"

#ifdef ON_SOLARIS
#include <strings.h>
#endif

#include <bitset>


using namespace std;

namespace solid{

//-----------------------------------------------------------------

/*static*/ const unsigned Debug::any(Debug::the().registerModule("ANY"));
/*static*/ const unsigned Debug::system(Debug::the().registerModule("SYSTEM"));
/*static*/ const unsigned Debug::specific(Debug::the().registerModule("SPECIFIC"));
/*static*/ const unsigned Debug::protocol(Debug::the().registerModule("PROTOCOL"));
/*static*/ const unsigned Debug::ser_bin(Debug::the().registerModule("SER_BIN"));
/*static*/ const unsigned Debug::utility(Debug::the().registerModule("UTILITY"));
/*static*/ const unsigned Debug::frame(Debug::the().registerModule("FRAME"));
/*static*/ const unsigned Debug::ipc(Debug::the().registerModule("FRAME_IPC"));
/*static*/ const unsigned Debug::tcp(Debug::the().registerModule("FRAME_TCP"));
/*static*/ const unsigned Debug::udp(Debug::the().registerModule("FRAME_UDP"));
/*static*/ const unsigned Debug::log(Debug::the().registerModule("LOG"));
/*static*/ const unsigned Debug::aio(Debug::the().registerModule("FRAME_AIO"));
/*static*/ const unsigned Debug::file(Debug::the().registerModule("FRAME_FILE"));

//-----------------------------------------------------------------

class DeviceOutBasicBuffer : public std::streambuf {
public:
	// constructor
	DeviceOutBasicBuffer(uint64 &_rsz): pd(NULL), sz(_rsz){}
	DeviceOutBasicBuffer(Device & _d, uint64 &_rsz) : pd(&_d), sz(_rsz){}
	void device(Device & _d){
		pd = &_d;
	}
	void close(){
		pd = NULL;
	}
protected:
	// write one character
	virtual
	int_type overflow(int_type c){
		if(c != EOF){
			char z = c;
			if(pd->write(&z, 1) != 1){
				return EOF;
			}
			++sz;
		}
		return c;
	}
	// write multiple characters
	virtual
	std::streamsize xsputn(const char* s, std::streamsize num){
		sz += num;
		return pd->write(s, static_cast<uint32>(num));
	}
private:
	Device		*pd;
	uint64		&sz;
};

//-----------------------------------------------------------------

class DeviceOutBuffer : public std::streambuf {
public:
	enum {BUFF_CP = 2048, BUFF_FLUSH = 1024};
	// constructor
	DeviceOutBuffer(uint64 &_rsz):pd(NULL), sz(_rsz), bpos(bbeg){}
	DeviceOutBuffer(Device & _d, uint64 &_rsz): pd(&_d), sz(_rsz), bpos(bbeg){}
	void device(Device & _d){
		pd = &_d;
	}
	void close(){
		if(pd == NULL || !pd->ok()) return;
		flush();
		pd = NULL;
	}
protected:
	// write one character
	virtual
	int_type overflow(int_type c);
	// write multiple characters
	virtual
	std::streamsize xsputn(const char* s, std::streamsize num);
private:
	bool flush(){
		int towrite = bpos - bbeg;
		bpos = bbeg;
		if(pd->write(bbeg, towrite) != towrite) return false;
		sz += towrite;
		return true;
	}
private:
	Device	*pd;
	uint64	&sz;
	char 	bbeg[BUFF_CP];
	char 	*bpos;
};

//-----------------------------------------------------------------

// virtual
std::streambuf::int_type DeviceOutBuffer::overflow(int_type c) {
	if (c != EOF){
		*bpos = c; ++bpos;
		if((bpos - bbeg) > BUFF_FLUSH && !flush()){
			return EOF;
		}
	}
	return c;
}
// write multiple characters
// virtual
std::streamsize DeviceOutBuffer::xsputn(const char* s, std::streamsize num){
	//we can safely put BUFF_FLUSH into the buffer
	int towrite = BUFF_CP - BUFF_FLUSH;
	if(towrite > static_cast<int>(num)) towrite = static_cast<int>(num);
	memcpy(bpos, s, towrite);
	bpos += towrite;
	if((bpos - bbeg) > BUFF_FLUSH && !flush()){
		cassert(0);
		return -1;
	}
	if(num == towrite) return num;
	num -= towrite;
	s += towrite;
	if(num >= BUFF_FLUSH){
		std::streamsize retv = pd->write(s, static_cast<uint32>(num));
		cassert(retv != num);
		return retv;
	}
	memcpy(bpos, s, static_cast<size_t>(num));
	bpos += num;
	return num;
}
//-----------------------------------------------------------------
class DeviceBasicOutStream : public std::ostream {
protected:
	DeviceOutBasicBuffer buf;
public:
	DeviceBasicOutStream(Device &_d, uint64 &_rsz) : std::ostream(0), buf(_d, _rsz) {
		rdbuf(&buf);
	}
	DeviceBasicOutStream(uint64 &_rsz):std::ostream(0), buf(_rsz){
		rdbuf(&buf);
	}
	void device(Device &_d){
		buf.device(_d);
	}
	void close(){
		buf.close();
	}
};
//-----------------------------------------------------------------
class DeviceOutStream : public std::ostream {
protected:
	DeviceOutBuffer buf;
public:
	DeviceOutStream(uint64 &_rsz):std::ostream(0), buf(_rsz){
		rdbuf(&buf);
	}
	DeviceOutStream(Device &_d, uint64 &_rsz) : std::ostream(0), buf(_d, _rsz) {
		rdbuf(&buf);
	}
	void device(Device &_d){
    	buf.device(_d);
    }
    void close(){
		buf.close();
	}
};
//-----------------------------------------------------------------
struct Debug::Data{
	typedef std::bitset<DEBUG_BITSET_SIZE>	BitSetT;
	typedef std::vector<const char*>		NameVectorT;
	Data():
		lvlmsk(0), sz(0), respinsz(0), respincnt(0),
		respinpos(0), dos(sz), dbos(sz), trace_debth(0)
	{
		pos = &std::cerr;
		bs.reset();
	}
	~Data(){
#ifdef ON_WINDOWS
		WSACleanup();
#endif
	}
	void setModuleMask(const char*);
	void setBit(const char *_pbeg, const char *_pend);
	bool initFile(uint32 _respincnt, uint64 _respinsz, string *_poutput);
	void doRespin();
	bool isActive()const{return lvlmsk != 0 && !bs.none();}
	Mutex					m;
	BitSetT					bs;
	unsigned				lvlmsk;
	NameVectorT				nv;
	uint64					sz;
	uint64					respinsz;
	uint32					respincnt;
	uint32					respinpos;
	DeviceOutStream			dos;
	DeviceBasicOutStream	dbos;
	FileDevice				fd;
	SocketDevice			sd;
	//std::ofstream			ofs;
	std::ostream			*pos;
	string					path;
	string					name;
	uint					trace_debth;
};
//-----------------------------------------------------------------
void splitPrefix(string &_path, string &_name, const char *_prefix);

#ifdef HAS_SAFE_STATIC
/*static*/ Debug& Debug::the(){
	static Debug d;
	return d;
}
#else

static Debug& Debug::dbg_the(){
	static Debug d;
	return d;
}

void Debug::once_cbk(){
	dbg_the();
}

/*static*/ Debug& Debug::the(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk, once);
	return dbg_the();
}


#endif
	
Debug::~Debug(){
	(*d.pos)<<flush;
	d.dos.close();
	d.dbos.close();
	d.pos = &cerr;
	//delete &d;
}

void Debug::Data::setBit(const char *_pbeg, const char *_pend){
	if(!cstring::ncasecmp(_pbeg, "all", _pend - _pbeg)){
		bs.set();
	}else if(!cstring::ncasecmp(_pbeg, "none", _pend - _pbeg)){
		bs.reset();
	}else for(NameVectorT::const_iterator it(nv.begin()); it != nv.end(); ++it){
		if(!cstring::ncasecmp(_pbeg, *it, _pend - _pbeg) && (int)strlen(*it) == (_pend - _pbeg)){
			bs.set(it - nv.begin());
		}
	}
}
uint32 parseLevels(const char *_lvl){
	if(!_lvl) return 0;
	uint32 r = 0;
	
	while(*_lvl){
		switch(*_lvl){
			case 'i':
			case 'I':
				r |= Debug::Info;
				break;
			case 'e':
			case 'E':
				r |= Debug::Error;
				break;
			case 'w':
			case 'W':
				r |= Debug::Warn;
				break;
			case 'r':
			case 'R':
				r |= Debug::Report;
				break;
			case 'v':
			case 'V':
				r |= Debug::Verbose;
				break;
			case 't':
			case 'T':
				r |= Debug::Trace;
				break;
		}
		++_lvl;
	}
	return r;
}

void Debug::Data::setModuleMask(const char *_opt){
	{
		Locker<Mutex> lock(m);
		bs.reset();
		if(_opt){
			const char *pbeg = _opt;
			const char *pcrt = _opt;
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
}

void filePath(string &_out, uint32 _pos, ulong _pid, const string &_path, const string &_name){
	_out = _path;
	_out += _name;
	
	char	buf[2048];
	
	if(_pos){
		sprintf(buf, "_%lu_%u.dbg", _pid, _pos);
	}else{
		sprintf(buf, "_%lu.dbg", _pid);
	}
	_out += buf;
}

bool Debug::Data::initFile(uint32 _respincnt, uint64 _respinsz, string *_poutput){
	respincnt = _respincnt;
	respinsz = _respinsz;
	respinpos = 0;
	string fpath;
#ifdef ON_WINDOWS
	ulong pid(_getpid());
#else
	ulong pid(getpid());
#endif
	filePath(fpath, 0, pid, path, name);
	if(fd.create(fpath.c_str(), FileDevice::WO)) return false;
	if(_poutput){
		*_poutput = fpath;
	}
	return true;
}

void Debug::Data::doRespin(){
	sz = 0;
	string fname;
#ifdef ON_WINDOWS
	ulong pid(_getpid());
#else
	ulong pid(getpid());
#endif

	//first we erase the oldest file:
	if(respinpos > respincnt){
		filePath(fname, respinpos - respincnt, pid, path, name);
		Directory::eraseFile(fname.c_str());
	}
	fname.clear();
	++respinpos;

	//rename the curent file name_PID.dbg to name_PID_RESPINPOS.dbg
	filePath(fname, respinpos, pid, path, name);
	string crtname;
	filePath(crtname, 0, pid, path, name);
	fd.close();
	if(pos == &dos){
		dos.device(fd);//close the current file
	}else if(pos == &dbos){
		dbos.device(fd);//close the current file
	}else{
		cassert(false);
	}
	Directory::renameFile(fname.c_str(), crtname.c_str());
	if(fd.create(crtname.c_str(), FileDevice::WO)){
		pos = &cerr;
	}else{
		if(pos == &dos){
			dos.device(fd);//close the current file
		}else if(pos == &dbos){
			dbos.device(fd);//close the current file
		}else{
			cassert(false);
		}
	}
}


void Debug::initStdErr(
	bool _buffered,
	std::string *_output
){
	Locker<Mutex> lock(d.m);

	d.dos.close();
	d.dbos.close();
    if(!d.isActive()) return;
	if(_buffered){
		d.pos = &std::clog;
		if(_output){
			*_output += "clog";
			*_output += " (buffered)";
		}
	}else{
		d.pos = &std::cerr;
		if(_output){
			*_output += "cerr";
			*_output += " (unbuffered)";
		}
	}
}

void Debug::initFile(
	const char * _prefix,
	bool _buffered,
	ulong _respincnt,
	ulong _respinsize,
	std::string *_output
){
	Locker<Mutex> lock(d.m);
	d.respinsz = 0;

	d.dos.close();
	d.dbos.close();
	if(!d.isActive()) return;
	
	if(_prefix && *_prefix){
		splitPrefix(d.path, d.name, _prefix);
		if(d.path.empty()){
			Directory::create("dbg");
			d.path = "dbg/";
		}
		if(d.initFile(_respincnt, _respinsize, _output)){
			if(_buffered){
				d.dos.device(d.fd);
				d.pos = &d.dos;
				if(_output){
					*_output += " (buffered)";
				}
			}else{
				d.dbos.device(d.fd);
				d.pos = &d.dbos;
				if(_output){
					*_output += " (unbuffered)";
				}
			}
			return;
		}
	}
	if(_buffered){
		d.pos = &std::clog;
		if(_output){
			*_output += "clog";
			*_output += " (buffered)";
		}
	}else{
		d.pos = &std::cerr;
		if(_output){
			*_output += "cerr";
			*_output += " (unbuffered)";
		}
	}
}
void Debug::initSocket(
	const char * _addr,
	const char * _port,
	bool _buffered,
	std::string *_output
){
	if(!d.isActive()) return;
	//do the connect outside locking
	ResolveData		rd = synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	if(!rd.empty() && d.sd.create(rd.begin()) == OK && d.sd.connect(rd.begin()) == OK){
	}else{
		d.sd.close();//make sure the socket is closed
	}
	
	Locker<Mutex> lock(d.m);
	d.respinsz = 0;
	
	d.dos.close();
	d.dbos.close();
	
	if(_addr == 0 || !*_addr){
		_addr = "localhost";
	}
	
	if(d.sd.ok()){
		if(_buffered){
			d.dos.device(d.sd);
			d.pos = &d.dos;
			if(_output){
				*_output += _addr;
				*_output += ':';
				*_output += _port;
				*_output += " (buffered)";
			}
		}else{
			d.dbos.device(d.sd);
			d.pos = &d.dbos;
			if(_output){
				*_output += _addr;
				*_output += ':';
				*_output += _port;
				*_output += " (unbuffered)";
			}
		}
	}else{
		if(_buffered){
			d.pos = &std::clog;
			if(_output){
				*_output += "clog";
				*_output += " (buffered)";
			}
		}else{
			d.pos = &std::cerr;
			if(_output){
				*_output += "cerr";
				*_output += " (unbuffered)";
			}
		}
	}
}

void Debug::levelMask(const char *_msk){
	Locker<Mutex> lock(d.m);
	if(!_msk){
		_msk = "iewrvt";
	}
	d.lvlmsk = parseLevels(_msk);
}
void Debug::moduleMask(const char *_msk){
	if(!_msk){
		_msk = "all";
	}
	d.setModuleMask(_msk);
}

void Debug::moduleBits(std::string &_ros){
	Locker<Mutex> lock(d.m);
	for(Data::NameVectorT::const_iterator it(d.nv.begin()); it != d.nv.end(); ++it){
		_ros += *it;
		_ros += ' ';
	}
}
void Debug::setAllModuleBits(){
	Locker<Mutex> lock(d.m);
	d.bs.set();
}
void Debug::resetAllModuleBits(){
	Locker<Mutex> lock(d.m);
	d.bs.reset();
}
void Debug::setModuleBit(unsigned _v){
	Locker<Mutex> lock(d.m);
	d.bs.set(_v);
}
void Debug::resetModuleBit(unsigned _v){
	Locker<Mutex> lock(d.m);
	d.bs.reset(_v);
}
unsigned Debug::registerModule(const char *_name){
	Locker<Mutex> lock(d.m);
	d.nv.push_back(_name);
	return d.nv.size() - 1;
}

std::ostream& Debug::print(){
	d.m.lock();
	return std::cerr;
}

std::ostream& Debug::print(
	const char _t,
	unsigned _module,
	const char *_file,
	const char *_fnc,
	int _line
){
	d.m.lock();
	if(d.respinsz && d.respinsz <= d.sz){
		d.doRespin();
	}
	char		buf[128];
	TimeSpec	ts_now(TimeSpec::createRealTime());
	time_t		t_now = ts_now.seconds();
	tm			*ploctm;
#ifdef ON_WINDOWS
	ploctm = localtime(&t_now);
#else
	tm			loctm;
	ploctm = localtime_r(&t_now, &loctm);
#endif
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u][%s]",
		_t,
		ploctm->tm_year + 1900,
		ploctm->tm_mon + 1, 
		ploctm->tm_mday,
		ploctm->tm_hour,
		ploctm->tm_min,
		ploctm->tm_sec,
		(uint)ts_now.nanoSeconds()/1000000,
		d.nv[_module]//,
		//Thread::currentId()
	);
#ifdef ON_WINDOWS
	return (*d.pos)<<buf<<'['<<src_file_name(_file)<<':'<<_line<<' '<<_fnc<<"]["<<Thread::currentId()<<']'<<' ';
#else
	return (*d.pos)<<buf<<'['<<src_file_name(_file)<<':'<<_line<<' '<<_fnc<<"][0x"<<std::hex<<Thread::currentId()<<std::dec<<']'<<' ';
#endif
}
static const char tabs[]="\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
						 "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
						 "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

DebugTraceTest::~DebugTraceTest(){
	if(v){
		Debug::the().printTraceOut('T', mod, file, fnc, line)<<"???";
		Debug::the().doneTraceOut();
	}
}

std::ostream& Debug::printTraceIn(
	const char _t,
	unsigned _module,
	const char *_file,
	const char *_fnc,
	int _line
){
	d.m.lock();
	if(d.respinsz && d.respinsz <= d.sz){
		d.doRespin();
	}
	char		buf[128];
	TimeSpec	ts_now(TimeSpec::createRealTime());
	time_t		t_now = ts_now.seconds();
	tm			*ploctm;
#ifdef ON_WINDOWS
	ploctm = localtime(&t_now);
#else
	tm			loctm;
	ploctm = localtime_r(&t_now, &loctm);
#endif
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u]",
		_t,
		ploctm->tm_year + 1900,
		ploctm->tm_mon + 1, 
		ploctm->tm_mday,
		ploctm->tm_hour,
		ploctm->tm_min,
		ploctm->tm_sec,
		(uint)ts_now.nanoSeconds()/1000000//,
		//d.nv[_module],
		//Thread::currentId()
	);
	(*d.pos)<<buf;
	d.pos->write(tabs, d.trace_debth);
	++d.trace_debth;
#ifdef ON_WINDOWS
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<src_file_name(_file)<<':'<<_line<<"]["<<Thread::currentId()<<']'<<' '<<_fnc<<'(';
#else
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<src_file_name(_file)<<':'<<_line<<"][0x"<<std::hex<<Thread::currentId()<<std::dec<<']'<<' '<<_fnc<<'(';
#endif
	return (*d.pos);
}

std::ostream& Debug::printTraceOut(
	const char _t,
	unsigned _module,
	const char *_file,
	const char *_fnc,
	int _line
){
	d.m.lock();
	if(d.respinsz && d.respinsz <= d.sz){
		d.doRespin();
	}
	char		buf[128];
	TimeSpec	ts_now(TimeSpec::createRealTime());
	time_t		t_now = ts_now.seconds();
	tm			*ploctm;
#ifdef ON_WINDOWS
	ploctm = localtime(&t_now);
#else
	tm			loctm;
	ploctm = localtime_r(&t_now, &loctm);
#endif
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u]",
		_t,
		ploctm->tm_year + 1900,
		ploctm->tm_mon + 1, 
		ploctm->tm_mday,
		ploctm->tm_hour,
		ploctm->tm_min,
		ploctm->tm_sec,
		(uint)ts_now.nanoSeconds()/1000000//,
		//d.nv[_module],
		//Thread::currentId()
	);
	(*d.pos)<<buf;
	--d.trace_debth;
	d.pos->write(tabs, d.trace_debth);
#ifdef ON_WINDOWS
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<src_file_name(_file)<<':'<<_line<<"]["<<Thread::currentId()<<']'<<' '<<'}'<<_fnc<<'(';
#else
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<src_file_name(_file)<<':'<<_line<<"][0x"<<std::hex<<Thread::currentId()<<std::dec<<']'<<' '<<'}'<<_fnc<<'(';
#endif
	return (*d.pos);
}

void Debug::done(){
	(*d.pos)<<std::endl;
	cassert(d.pos->good());
	d.m.unlock();
}

void Debug::doneTraceIn(){
	(*d.pos)<<')'<<'{'<<std::endl;
	d.m.unlock();
}

void Debug::doneTraceOut(){
	(*d.pos)<<')'<<std::endl;
	d.m.unlock();
}

bool Debug::isSet(Level _lvl, unsigned _v)const{
	return (d.lvlmsk & _lvl) && _v < d.bs.size() && d.bs[_v];
}
Debug::Debug():d(*(new Data)){
	resetAllModuleBits();
	levelMask();
}

void splitPrefix(string &_path, string &_name, const char *_prefix){
	const char *p = strrchr(_prefix, '/');
	if(!p){
		_name = _prefix;
	}else{
		_path.assign(_prefix, (p - _prefix) + 1);
		_name = (p + 1);
	}
}

}//namespace solid
