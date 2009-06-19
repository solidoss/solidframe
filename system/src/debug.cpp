/* Implementation file debug.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef UDEBUG

#define DO_EXPORT_DLL 1
#include "debug.hpp"

#include "directory.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <bitset>
#include "system/timespec.hpp"
#include "system/socketdevice.hpp"
#include "system/socketaddress.hpp"
#include "system/filedevice.hpp"
#include "cassert.hpp"
#include "thread.hpp"
#include "mutex.hpp"

#ifdef ON_SUN
#include <strings.h>
#endif


const unsigned fileoff = (strlen(__FILE__) - strlen(strstr(__FILE__, "system/src")));

using namespace std;

//-----------------------------------------------------------------

/*static*/ const unsigned Dbg::any(Dbg::instance().registerModule("ANY"));
/*static*/ const unsigned Dbg::system(Dbg::instance().registerModule("SYSTEM"));
/*static*/ const unsigned Dbg::specific(Dbg::instance().registerModule("SPECIFIC"));
/*static*/ const unsigned Dbg::protocol(Dbg::instance().registerModule("PROTOCOL"));
/*static*/ const unsigned Dbg::ser_bin(Dbg::instance().registerModule("SER_BIN"));
/*static*/ const unsigned Dbg::utility(Dbg::instance().registerModule("UTILITY"));
/*static*/ const unsigned Dbg::cs(Dbg::instance().registerModule("FDT"));
/*static*/ const unsigned Dbg::ipc(Dbg::instance().registerModule("FDT_IPC"));
/*static*/ const unsigned Dbg::tcp(Dbg::instance().registerModule("FDT_TCP"));
/*static*/ const unsigned Dbg::udp(Dbg::instance().registerModule("FDT_UDP"));
/*static*/ const unsigned Dbg::filemanager(Dbg::instance().registerModule("FDT_FILEMANAGER"));
/*static*/ const unsigned Dbg::log(Dbg::instance().registerModule("LOG"));
/*static*/ const unsigned Dbg::aio(Dbg::instance().registerModule("FDT_AIO"));

//-----------------------------------------------------------------

class DeviceOutBasicBuffer : public std::streambuf {
public:
	// constructor
	DeviceOutBasicBuffer(uint64 &_rsz):sz(_rsz){}
	DeviceOutBasicBuffer(const Device & _d, uint64 &_rsz) : d(_d), sz(_rsz){}
	void device(const Device & _d){
		d = _d;
	}
	void close(){
		d.close();
	}
protected:
	// write one character
	virtual
	int_type overflow(int_type c){
		if(c != EOF){
			char z = c;
			if(d.write(&z, 1) != 1){
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
		return d.write(s, num);
	}
private:
	Device		d;
	uint64		&sz;
};

//-----------------------------------------------------------------

class DeviceOutBuffer : public std::streambuf {
public:
	enum {BUFF_CP = 2048, BUFF_FLUSH = 1024};
	// constructor
	DeviceOutBuffer(uint64 &_rsz):sz(_rsz), bpos(bbeg){}
	DeviceOutBuffer(const Device & _d, uint64 &_rsz): d(_d), sz(_rsz), bpos(bbeg){}
	void device(const Device & _d){
		d = _d;
	}
	void close(){
		d.close();
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
		if(d.write(bbeg, towrite) != towrite) return false;
		sz += towrite;
		return true;
	}
private:
	Device	d;
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
	if(towrite > num) towrite = num;
	memcpy(bpos, s, towrite);
	bpos += towrite;
	if((bpos - bbeg) > BUFF_FLUSH && !flush()){
		return -1;
	}
	if(num == towrite) return num;
	num -= towrite;
	s += towrite;
	if(num >= BUFF_FLUSH){
		return d.write(s, num);
	}
	memcpy(bpos, s, num);
	bpos += num;
	return num;
}
//-----------------------------------------------------------------
class DeviceBasicOutStream : public std::ostream {
protected:
	DeviceOutBasicBuffer buf;
public:
	DeviceBasicOutStream(const Device &_d, uint64 &_rsz) : std::ostream(0), buf(_d, _rsz) {
		rdbuf(&buf);
	}
	DeviceBasicOutStream(uint64 &_rsz):std::ostream(0), buf(_rsz){
		rdbuf(&buf);
	}
	void device(const Device &_d){
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
	DeviceOutStream(const Device &_d, uint64 &_rsz) : std::ostream(0), buf(_d, _rsz) {
		rdbuf(&buf);
	}
	void device(const Device &_d){
    	buf.device(_d);
    }
    void close(){
		buf.close();
	}
};
//-----------------------------------------------------------------
struct Dbg::Data{
	typedef std::bitset<DEBUG_BITSET_SIZE>	BitSetTp;
	typedef std::vector<const char*>		NameVectorTp;
	Data():lvlmsk(0), sz(0), respinsz(0), respincnt(0), respinpos(0), dos(sz), dbos(sz), trace_debth(0){
		pos = &std::cerr;
	}
	void setModuleMask(const char*);
	void setBit(const char *_pbeg, const char *_pend);
	bool initFile(FileDevice &_rfd, uint32 _respincnt, uint64 _respinsz, string *_poutput);
	void doRespin();
	bool isActive()const{return lvlmsk != 0 && !bs.none();}
	Mutex					m;
	BitSetTp				bs;
	unsigned				lvlmsk;
	NameVectorTp			nv;
	time_t					begt;
	TimeSpec				begts;
	uint64					sz;
	uint64					respinsz;
	uint32					respincnt;
	uint32					respinpos;
	DeviceOutStream			dos;
	DeviceBasicOutStream	dbos;
	//std::ofstream			ofs;
	std::ostream			*pos;
	string					path;
	string					name;
	uint					trace_debth;
};
//-----------------------------------------------------------------
void splitPrefix(string &_path, string &_name, const char *_prefix);

/*static*/ Dbg& Dbg::instance(){
	static Dbg d;
	return d;
}
	
Dbg::~Dbg(){
	delete &d;
}

void Dbg::Data::setBit(const char *_pbeg, const char *_pend){
	if(!strncasecmp(_pbeg, "all", _pend - _pbeg)){
		bs.set();
	}else if(!strncasecmp(_pbeg, "none", _pend - _pbeg)){
		bs.reset();
	}else for(NameVectorTp::const_iterator it(nv.begin()); it != nv.end(); ++it){
		if(!strncasecmp(_pbeg, *it, _pend - _pbeg) && (int)strlen(*it) == (_pend - _pbeg)){
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
				r |= Dbg::Info;
				break;
			case 'e':
			case 'E':
				r |= Dbg::Error;
				break;
			case 'w':
			case 'W':
				r |= Dbg::Warn;
				break;
			case 'r':
			case 'R':
				r |= Dbg::Report;
				break;
			case 'v':
			case 'V':
				r |= Dbg::Verbose;
				break;
			case 't':
			case 'T':
				r |= Dbg::Trace;
				break;
		}
		++_lvl;
	}
	return r;
}

void Dbg::Data::setModuleMask(const char *_opt){
	{
		Mutex::Locker lock(m);
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
	char buf[128];
	if(_pos){
		sprintf(buf, "_%u_%u.dbg", _pid, _pos);
	}else{
		sprintf(buf, "_%u.dbg", _pid);
	}
	_out += buf;
}

bool Dbg::Data::initFile(FileDevice &_rfd, uint32 _respincnt, uint64 _respinsz, string *_poutput){
	respincnt = _respincnt;
	respinsz = _respinsz;
	respinpos = 0;
	string fpath;
	filePath(fpath, 0, getpid(), path, name);
	if(_rfd.create(fpath.c_str(), FileDevice::WO)) return false;
	if(_poutput){
		*_poutput = fpath;
	}
	return true;
}

void Dbg::Data::doRespin(){
	sz = 0;
	string fname;
	//first we erase the oldest file:
	if(respinpos > respincnt){
		filePath(fname, respinpos - respincnt, getpid(), path, name);
		Directory::eraseFile(fname.c_str());
	}
	fname.clear();
	++respinpos;
	//rename the curent file name_PID.dbg to name_PID_RESPINPOS.dbg
	filePath(fname, respinpos, getpid(), path, name);
	string crtname;
	filePath(crtname, 0, getpid(), path, name);
	FileDevice fd;
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


void Dbg::initStdErr(
	bool _buffered,
	std::string *_output
){
	Mutex::Locker lock(d.m);

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

void Dbg::initFile(
	const char * _prefix,
	bool _buffered,
	ulong _respincnt,
	ulong _respinsize,
	std::string *_output
){
	Mutex::Locker lock(d.m);
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
		FileDevice fd;
		if(d.initFile(fd, _respincnt, _respinsize, _output)){
			if(_buffered){
				d.dos.device(fd);
				d.pos = &d.dos;
				if(_output){
					*_output += " (buffered)";
				}
			}else{
				d.dbos.device(fd);
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
void Dbg::initSocket(
	const char * _addr,
	const char * _port,
	bool _buffered,
	std::string *_output
){
	if(!d.isActive()) return;
	//do the connect outside locking
	AddrInfo ai(_addr, _port);
	SocketDevice sd;
	if(!ai.empty() && sd.create(ai.begin()) == OK && sd.connect(ai.begin()) == OK){
	}else{
		sd.close();//make sure the socket is closed
	}
	
	Mutex::Locker lock(d.m);
	d.respinsz = 0;
	
	d.dos.close();
	d.dbos.close();
	
	if(_addr == 0 || !*_addr){
		_addr = "localhost";
	}
	
	if(sd.ok()){
		if(_buffered){
			d.dos.device(sd);
			d.pos = &d.dos;
			if(_output){
				*_output += _addr;
				*_output += ':';
				*_output += _port;
				*_output += " (buffered)";
			}
		}else{
			d.dbos.device(sd);
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

void Dbg::levelMask(const char *_msk){
	Mutex::Locker lock(d.m);
	if(!_msk){
		_msk = "iewrvt";
	}
	d.lvlmsk = parseLevels(_msk);
}
void Dbg::moduleMask(const char *_msk){
	if(!_msk){
		_msk = "all";
	}
	d.setModuleMask(_msk);
}

void Dbg::moduleBits(std::string &_ros){
	Mutex::Locker lock(d.m);
	for(Data::NameVectorTp::const_iterator it(d.nv.begin()); it != d.nv.end(); ++it){
		_ros += *it;
		_ros += ' ';
	}
}
void Dbg::setAllModuleBits(){
	Mutex::Locker lock(d.m);
	d.bs.set();
}
void Dbg::resetAllModuleBits(){
	Mutex::Locker lock(d.m);
	d.bs.reset();
}
void Dbg::setModuleBit(unsigned _v){
	Mutex::Locker lock(d.m);
	d.bs.set(_v);
}
void Dbg::resetModuleBit(unsigned _v){
	Mutex::Locker lock(d.m);
	d.bs.reset(_v);
}
unsigned Dbg::registerModule(const char *_name){
	Mutex::Locker lock(d.m);
	d.nv.push_back(_name);
	return d.nv.size() - 1;
}

std::ostream& Dbg::print(){
	d.m.lock();
	return std::cerr;
}

std::ostream& Dbg::print(
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
	char buf[128];
	TimeSpec ts_now;
	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	ts_now = ts_now - d.begts;
	time_t t_now = d.begt + ts_now.seconds();
	tm loctm;
	localtime_r(&t_now, &loctm);
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u][%s]",
		_t,
		loctm.tm_year + 1900,
		loctm.tm_mon + 1, 
		loctm.tm_mday,
		loctm.tm_hour,
		loctm.tm_min,
		loctm.tm_sec,
		(uint)ts_now.nanoSeconds()/1000000,
		d.nv[_module]//,
		//Thread::currentId()
	);
	return (*d.pos)<<buf<<'['<<_file + fileoff<<'('<<_line<<')'<<' '<<_fnc<<"][0x"<<std::hex<<Thread::currentId()<<std::dec<<']'<<' ';
}
static const char tabs[]="\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
						 "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
						 "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

#ifdef UTRACE
DbgTraceTest::~DbgTraceTest(){
	if(v){
		Dbg::instance().printTraceOut('T', mod, file, fnc, line)<<"???";
		Dbg::instance().doneTraceOut();
	}
}
#endif

std::ostream& Dbg::printTraceIn(
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
	char buf[128];
	TimeSpec ts_now;
	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	ts_now = ts_now - d.begts;
	time_t t_now = d.begt + ts_now.seconds();
	tm loctm;
	localtime_r(&t_now, &loctm);
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u]",
		_t,
		loctm.tm_year + 1900,
		loctm.tm_mon + 1, 
		loctm.tm_mday,
		loctm.tm_hour,
		loctm.tm_min,
		loctm.tm_sec,
		(uint)ts_now.nanoSeconds()/1000000//,
		//d.nv[_module],
		//Thread::currentId()
	);
	(*d.pos)<<buf;
	d.pos->write(tabs, d.trace_debth);
	++d.trace_debth;
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<_file + fileoff<<'('<<_line<<')'<<"][0x"<<std::hex<<Thread::currentId()<<std::dec<<']'<<' '<<_fnc<<'(';
	return (*d.pos);
}

std::ostream& Dbg::printTraceOut(
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
	char buf[128];
	TimeSpec ts_now;
	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	ts_now = ts_now - d.begts;
	time_t t_now = d.begt + ts_now.seconds();
	tm loctm;
	localtime_r(&t_now, &loctm);
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u]",
		_t,
		loctm.tm_year + 1900,
		loctm.tm_mon + 1, 
		loctm.tm_mday,
		loctm.tm_hour,
		loctm.tm_min,
		loctm.tm_sec,
		(uint)ts_now.nanoSeconds()/1000000//,
		//d.nv[_module],
		//Thread::currentId()
	);
	(*d.pos)<<buf;
	--d.trace_debth;
	d.pos->write(tabs, d.trace_debth);
	(*d.pos)<<'['<<d.nv[_module]<<']'<<'['<<_file + fileoff<<'('<<_line<<')'<<']'<<'['<<Thread::currentId()<<']'<<' '<<'}'<<_fnc<<'(';
	return (*d.pos);
}

void Dbg::done(){
	(*d.pos)<<std::endl;
	d.m.unlock();
}

void Dbg::doneTraceIn(){
	(*d.pos)<<')'<<'{'<<std::endl;
	d.m.unlock();
}

void Dbg::doneTraceOut(){
	(*d.pos)<<')'<<std::endl;
	d.m.unlock();
}

bool Dbg::isSet(Level _lvl, unsigned _v)const{
	return (d.lvlmsk & _lvl) && d.bs[_v];
}
Dbg::Dbg():d(*(new Data)){
	d.begt = time(NULL);
	clock_gettime(CLOCK_MONOTONIC, &d.begts);
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

#endif


