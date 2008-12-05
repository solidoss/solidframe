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
#include "cassert.hpp"
#include "thread.hpp"
#include "mutex.hpp"


const unsigned fileoff = (strstr(__FILE__, "system/src") - __FILE__);

#define OUTS std::clog

/*static*/ const unsigned Dbg::any(Dbg::instance().registerModule("ANY"));
/*static*/ const unsigned Dbg::system(Dbg::instance().registerModule("SYSTEM"));
/*static*/ const unsigned Dbg::specific(Dbg::instance().registerModule("SPECIFIC"));
/*static*/ const unsigned Dbg::protocol(Dbg::instance().registerModule("PROTOCOL"));
/*static*/ const unsigned Dbg::ser_bin(Dbg::instance().registerModule("SER_BIN"));
/*static*/ const unsigned Dbg::utility(Dbg::instance().registerModule("UTILITY"));
/*static*/ const unsigned Dbg::cs(Dbg::instance().registerModule("CS"));
/*static*/ const unsigned Dbg::ipc(Dbg::instance().registerModule("CS_IPC"));
/*static*/ const unsigned Dbg::tcp(Dbg::instance().registerModule("CS_TCP"));
/*static*/ const unsigned Dbg::udp(Dbg::instance().registerModule("CS_UDP"));
/*static*/ const unsigned Dbg::filemanager(Dbg::instance().registerModule("CS_FILEMANAGER"));
/*static*/ const unsigned Dbg::log(Dbg::instance().registerModule("LOG"));
/*static*/ const unsigned Dbg::aio(Dbg::instance().registerModule("CS_AIO"));

class DeviceOutBasicBuffer : public std::streambuf {
public:
	// constructor
	DeviceOutBasicBuffer(){}
	DeviceOutBasicBuffer(const Device & _d) : d(_d){}
	void device(const Device & _d){
		d = _d;
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
		}
		return c;
	}
	// write multiple characters
	virtual
	std::streamsize xsputn(const char* s, std::streamsize num){
		return d.write(s, num);
	}
private:
	Device d;
};

class DeviceOutBuffer : public std::streambuf {
public:
	enum {BUFF_CP = 2048, BUFF_FLUSH = 1024};
	// constructor
	DeviceOutBuffer():bpos(bbeg){}
	DeviceOutBuffer(const Device & _d) : d(_d), bpos(bbeg){}
	void device(const Device & _d){
		d = _d;
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
		uint towrite = bpos - bbeg;
		bpos = bbeg;
		if(d.write(bbeg, towrite) != towrite) return false;
		return true;
	}
private:
	Device	d;
	char 	bbeg[BUFF_CP];
	char 	*bpos;
};

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
	uint towrite = BUFF_CP - BUFF_FLUSH;
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

class DeviceBasicOutStream : public std::ostream {
protected:
	DeviceOutBasicBuffer buf;
public:
	DeviceBasicOutStream(const Device &_d) : std::ostream(0), buf(_d) {
		rdbuf(&buf);
	}
	DeviceBasicOutStream():std::ostream(0){
		rdbuf(&buf);
	}
	void device(const Device &_d){
		buf.device(_d);
	}
};

class DeviceOutStream : public std::ostream {
protected:
	DeviceOutBuffer buf;
public:
	DeviceOutStream():std::ostream(0){
		rdbuf(&buf);
	}
	DeviceOutStream(const Device &_d) : std::ostream(0), buf(_d) {
		rdbuf(&buf);
	}
	void device(const Device &_d){
    	buf.device(_d);
    }
};

struct Dbg::Data{
	typedef std::bitset<DEBUG_BITSET_SIZE>	BitSetTp;
	typedef std::vector<const char*>		NameVectorTp;
	Data():lvlmsk(0){
		pos = &std::cerr;
	}
	bool init(uint32, const char*);
	void setBit(const char *_pbeg, const char *_pend);
	Mutex					m;
	BitSetTp				bs;
	unsigned				lvlmsk;
	NameVectorTp			nv;
	time_t					begt;
	TimeSpec				begts;
	DeviceOutStream			dos;
	DeviceBasicOutStream	dbos;
	std::ofstream			ofs;
	std::ostream			*pos;
};

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
		if(!strncasecmp(_pbeg, *it, _pend - _pbeg) && strlen(*it) == (_pend - _pbeg)){
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
		}
		++_lvl;
	}
	return r;
}

bool Dbg::Data::init(uint32	_lvlopt, const char *_opt){
	{
		Mutex::Locker lock(m);
		lvlmsk = _lvlopt;
		if(lvlmsk == 0) return true;//
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
		if(bs.none()) return true;
	}
	return false;
}

void Dbg::init(
	std::string &_file,
	const char * _prefix,
	const char *_lvlopt,
	const char *_opt,
	bool _buffered
){
	init(_file, _prefix, parseLevels(_lvlopt), _opt, _buffered);
}

void Dbg::init(
	std::string &_file,
	const char * _prefix,
	uint32	_lvlopt,
	const char *_opt,
	bool _buffered
){
	if(d.init(_lvlopt, _opt)) return;
	
	if(_prefix && *_prefix){
		Directory::create("dbg");
		char *name = new char[strlen(_prefix)+50];
		sprintf(name,"dbg/%s_%u.dbg", _prefix, getpid());
		//printf("Debug file: [%s]\r\n", name);
		_file = name;
// 		int fd = open(name, O_CREAT | O_RDWR | O_TRUNC, 0600);
// 		delete []name;
// 		if(dup2(fd, fileno(stderr))<0){
// 			printf("error duplicating filedescriptor\n");
// 		}
		d.ofs.open(name);
		d.pos = &d.ofs;
	}else{
		if(_buffered){
			d.pos = &std::clog;
		}else{
			d.pos = &std::cerr;
		}
	}
}

void Dbg::init(
	std::string &_file,
	const char * _addr,
	const char * _port,
	const char * _lvlopt,
	const char *_modopt,
	bool _buffered
){
	init(_file, _addr, _port, parseLevels(_lvlopt), _modopt, _buffered);
}
void Dbg::init(
	std::string &_file,
	const char * _addr,
	const char * _port,
	unsigned  _lvlopt,
	const char *_modopt,
	bool _buffered
){
	if(d.init(_lvlopt, _modopt)) return;
	
	if(_addr == 0 || !*_addr){
		_addr = "localhost";
	}
	
	//cassert(_port && *_port);
	
	AddrInfo ai(_addr, _port);
	SocketDevice sd;
	if(!ai.empty() && sd.create(ai.begin()) == OK && sd.connect(ai.begin()) == OK){
		if(_buffered){
			d.dos.device(sd);
			d.pos = &d.dos;
		}else{
			d.dbos.device(sd);
			d.pos = &d.dbos;
		}
	}else{
		if(_buffered){
			d.pos = &std::clog;
		}else{
			d.pos = &std::cerr;
		}
	}
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
	char buf[128];
	TimeSpec ts_now;
	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	ts_now = ts_now - d.begts;
	time_t t_now = d.begt + ts_now.seconds();
	tm loctm;
	localtime_r(&t_now, &loctm);
	sprintf(
		buf,
		"%c[%04u-%02u-%02u %02u:%02u:%02u.%03u][%s][%d]",
		_t,
		loctm.tm_year + 1900,
		loctm.tm_mon, 
		loctm.tm_mday,
		loctm.tm_hour,
		loctm.tm_min,
		loctm.tm_sec,
		ts_now.nanoSeconds()/1000000,
		d.nv[_module],
		Thread::currentId()
	);
	return (*d.pos)<<buf<<'['<<_file + fileoff<<'('<<_line<<')'<<' '<<_fnc<<']'<<' ';
}

void Dbg::done(){
	(*d.pos)<<std::endl;
	d.m.unlock();
}
bool Dbg::isSet(Level _lvl, unsigned _v)const{
	return (d.lvlmsk & _lvl) && d.bs[_v];
}
Dbg::Dbg():d(*(new Data)){
	d.begt = time(NULL);
	clock_gettime(CLOCK_MONOTONIC, &d.begts);
}

#endif


