/* Implementation file listenerselector.cpp
	
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

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>

#include "system/cassert.h"
#include "system/debug.h"
#include "system/timespec.h"

#include "core/common.h"

#include "tcp/station.h"
#include "tcp/listenerselector.h"
#include "tcp/listener.h"

namespace clientserver{
namespace tcp{

ListenerSelector::ListenerSelector():cp(0),sz(0),pss(NULL),pfds(0){
	pipefds[0] = pipefds[1] = -1;
}
ListenerSelector::~ListenerSelector(){
	delete []pss;
	delete []pfds;
}
int ListenerSelector::reserve(ulong _cp){
	pfds = new pollfd[_cp];
	pss  = new SelStation[_cp];
	cp = _cp;
	if(pipefds[0] < 0){
		if(pipe(pipefds)) return BAD;
		fcntl(pipefds[0],F_SETFL, O_NONBLOCK);
		sz = 1;
		pfds->fd = pipefds[0];
		pfds->events = POLLIN;
		pfds->revents = 0;
	}
	return OK;
}
void ListenerSelector::signal(uint _pos){
	idbg("signal listener selector "<<_pos);
	write(pipefds[1], &_pos, sizeof(uint32));
}

void ListenerSelector::run(){
	int state;
	do{
		state = 0;
		int selcnt = poll(pfds, sz, -1);
		if(pfds->revents) state = doReadPipe();
		if(state & FULL_SCAN)
			state |= doFullScan();
		else
			state |= doSimpleScan(selcnt);
	}while(!(state & EXIT_LOOP));
}

void ListenerSelector::push(const ListenerPtrTp &_rlis, uint _thid){
	pfds[sz].fd = _rlis->station().descriptor();
	_rlis->setThread(_thid, sz);
	pfds[sz].events = POLLIN;
	pfds[sz].revents = 0;
	pss[sz].lisptr = _rlis;
	++sz;
}

int ListenerSelector::doReadPipe(){
	enum {BUFSZ = 32, BUFLEN = BUFSZ * sizeof(ulong)};
	uint32	buf[BUFSZ];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	while((++j < maxcnt) && ((rsz = read(pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			if(buf[i]){
				rv |= FULL_SCAN;	
			}else{ 
				rv |= EXIT_LOOP;
			}
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			idbg("buf["<<i<<"]="<<buf[i]);
			if(buf[i]){
				rv |= FULL_SCAN;
			}else rv = EXIT_LOOP;//must exit
		}
	}
	if(j == maxcnt){
		//dummy read:
		rv = EXIT_LOOP | FULL_SCAN;//scan all filedescriptors for events
		idbg("reading dummy");
		while((rsz = read(pipefds[0],buf,BUFSZ)) > 0);
	}
	idbg("readpiperv = "<<rv);
	return rv;
}
//full scan the listeners for io signals and other singnals
int ListenerSelector::doFullScan(){
	uint oldsz = sz;
	TimeSpec ts;
	for(uint i = 1; i < sz;){
		if(pfds[i].revents || pss[i].lisptr->signaled(S_RAISE)){
			//TODO: make it use the exec(uint32, TimeSpec)
			switch(pss[i].lisptr->execute(0, ts)){
				case BAD:
					idbg("BAD: deleting listener");
					delete pss[i].lisptr.release();
					pss[i].lisptr = pss[sz - 1].lisptr;
					pfds[i] = pfds[sz - 1];
					--sz;
					break;
				case OK:
					idbg("OK: on listener");
				case NOK:
					idbg("NOK: on listener");
					++i;
					break;
				default:
					cassert(false);
			}
		}else ++i;
	}
	if(empty() || oldsz == cp && sz < oldsz) return EXIT_LOOP;
	return 0;
}
//scan only for io signals
int ListenerSelector::doSimpleScan(int _cnt){
	uint oldsz = sz;
	TimeSpec ts;
	for(uint i = 1; i < sz && _cnt;){
		if(pfds[i].revents){
			--_cnt;
			//TODO: make it use the execute(uint32, TimeSpec)
			switch(pss[i].lisptr->execute(0, ts)){
				case BAD:
					idbg("BAD: deleting listener");
					delete pss[i].lisptr.release();
					pss[i].lisptr = pss[sz - 1].lisptr;
					pfds[i] = pfds[sz - 1];
					--sz;
					break;
				case OK:
					idbg("OK: on listener");
				case NOK:
					idbg("NOK: on listener");
					++i;
					break;
				default:
					cassert(false);
			}
		}else ++i;
	}
	if(empty() || oldsz == cp && sz < oldsz) return EXIT_LOOP;
	return 0;
}

}//namespace tcp
}//namespace clientserver
