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

#include "debug.hpp"
#include "directory.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "cassert.hpp"
#include "thread.hpp"
#include "mutex.hpp"

#ifdef UDEBUG
const int Dbg::fileoff = (strstr(__FILE__, "system/src") - __FILE__);//sizeof("workspace/") - 1 == 9
#endif

#ifdef UDEBUG
#include <bitset>
typedef std::bitset<DEBUG_BITSET_SIZE>	DebugBitSetTp;
typedef std::vector<const char*>		DebugNameVectorTp;
static DebugBitSetTp& getBitSet(){
	static DebugBitSetTp bs;
	return bs;
}

static DebugNameVectorTp& getNameVector(){
	static DebugNameVectorTp nv;
	return nv;
}

Mutex& dbgMutex(){
	static Mutex gmut;
	return gmut;
}

unsigned registerDebugModule(const char *_name){
	Mutex::Locker lock(dbgMutex());
	getNameVector().push_back(_name);
	return getNameVector().size() - 1;
}

/*static*/ const unsigned Dbg::any(registerDebugModule("ANY"));
/*static*/ const unsigned Dbg::specific(registerDebugModule("SPECIFIC"));
/*static*/ const unsigned Dbg::system(registerDebugModule("SYSTEM"));
/*static*/ const unsigned Dbg::protocol(registerDebugModule("PROTOCOL"));
/*static*/ const unsigned Dbg::ser_bin(registerDebugModule("SER_BIN"));
/*static*/ const unsigned Dbg::utility(registerDebugModule("UTILITY"));
/*static*/ const unsigned Dbg::cs(registerDebugModule("CS"));
/*static*/ const unsigned Dbg::ipc(registerDebugModule("CS_IPC"));
/*static*/ const unsigned Dbg::tcp(registerDebugModule("CS_TCP"));
/*static*/ const unsigned Dbg::udp(registerDebugModule("CS_UDP"));
/*static*/ const unsigned Dbg::filemanager(registerDebugModule("CS_FILEMANAGER"));


void setAllDebugBits(){
	Mutex::Locker lock(dbgMutex());
	getBitSet().set();
}
void resetAllDebugBits(){
	Mutex::Locker lock(dbgMutex());
	getBitSet().reset();
}

void setDebugBit(unsigned _v){
	Mutex::Locker lock(dbgMutex());
	getBitSet().set(_v);
}
void resetDebugBit(unsigned _v){
	Mutex::Locker lock(dbgMutex());
	getBitSet().reset(_v);
}

bool Dbg::lock(TimeSpec &t){
	if(getBitSet()[any]){
		clock_gettime(CLOCK_MONOTONIC, &t);
		dbgMutex().lock();
		return true;
	}else{
		return false;
	}
}
const char* Dbg::lock(unsigned _v, TimeSpec &t){
	if(getBitSet()[_v]){
		clock_gettime(CLOCK_MONOTONIC, &t);
		dbgMutex().lock();
		cassert(getNameVector().size() > _v);
		const char *name = getNameVector()[_v];
		return name ? name : "UNKNOWN_MODULE";
	}else{
		return NULL;
	}
}

void Dbg::unlock(){
	dbgMutex().unlock();
}

long Dbg::crtThrId(){
	return Thread::currentId();
}

void printDebugBits(){
	using namespace std;
	Mutex::Locker lock(dbgMutex());
	cout<<"ALL NONE ";
	for(DebugNameVectorTp::const_iterator it(getNameVector().begin()); it != getNameVector().end(); ++it){
		cout<<*it<<' ';
	}
}

void setBit(const char *_pbeg, const char *_pend){
	if(!strncasecmp(_pbeg, "all", _pend - _pbeg)){
		getBitSet().set();
	}else if(!strncasecmp(_pbeg, "none", _pend - _pbeg)){
		getBitSet().reset();
	}else for(DebugNameVectorTp::const_iterator it(getNameVector().begin()); it != getNameVector().end(); ++it){
		if(!strncasecmp(_pbeg, *it, _pend - _pbeg) && strlen(*it) == (_pend - _pbeg)){
			getBitSet().set(it - getNameVector().begin());
		}
	}
}

#endif


void initDebug(const char *_prefix, const char *_opt){
	#ifdef UDEBUG
	std::ios_base::sync_with_stdio (false);
	Directory::create("dbg");
	char *name = new char[strlen(_prefix)+50];
	sprintf(name,"%s%u.dbg",_prefix,getpid());
	printf("debug file: [%s]\r\n",name);
	int fd = open(name, O_CREAT | O_RDWR | O_TRUNC, 0600);
	delete []name;
	if(dup2(fd,fileno(stderr))<0){
		printf("error duplicating filedescriptor\n");
	}
	if(_opt){
		Mutex::Locker lock(dbgMutex());
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
	#endif
}

