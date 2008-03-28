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

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"

#include "core/common.hpp"

#include "tcp/station.hpp"
#include "tcp/listenerselector.hpp"
#include "tcp/listener.hpp"

namespace clientserver{
namespace tcp{

struct ListenerSelector::Data{
	enum {EXIT_LOOP = 1, FULL_SCAN = 2, READ_PIPE = 4};
	struct SelStation{
		ListenerPtrTp	lisptr;
	};
	Data();
	~Data();
	uint			cp;
	uint			sz;
	SelStation		*pss;
	pollfd			*pfds;
	int				pipefds[2];
};

ListenerSelector::Data::Data():cp(0),sz(0),pss(NULL),pfds(0){
	pipefds[0] = pipefds[1] = -1;
}

ListenerSelector::Data::~Data(){
	close(pipefds[0]);
	close(pipefds[1]);
	delete []pss;
	delete []pfds;
}
ListenerSelector::ListenerSelector():d(*(new Data)){
}
ListenerSelector::~ListenerSelector(){
	delete &d;
}
int ListenerSelector::reserve(ulong _cp){
	d.pfds = new pollfd[_cp];
	d.pss  = new Data::SelStation[_cp];
	d.cp = _cp;
	if(d.pipefds[0] < 0){
		if(pipe(d.pipefds)) return BAD;
		fcntl(d.pipefds[0],F_SETFL, O_NONBLOCK);
		d.sz = 1;
		d.pfds->fd = d.pipefds[0];
		d.pfds->events = POLLIN;
		d.pfds->revents = 0;
	}
	return OK;
}

uint ListenerSelector::capacity()const{
	return d.cp - 1;
}
uint ListenerSelector::size() const{
	return d.sz;
}
int  ListenerSelector::empty()const{
	return d.sz == 1;
}
int  ListenerSelector::full()const{
	return d.sz == d.cp;
}

void ListenerSelector::prepare(){
}
void ListenerSelector::unprepare(){
}

void ListenerSelector::signal(uint _pos){
	idbg("signal listener selector "<<_pos);
	write(d.pipefds[1], &_pos, sizeof(uint32));
}

void ListenerSelector::run(){
	int state;
	do{
		state = 0;
		int selcnt = poll(d.pfds, d.sz, -1);
		if(d.pfds->revents) state = doReadPipe();
		if(state & Data::FULL_SCAN)
			state |= doFullScan();
		else
			state |= doSimpleScan(selcnt);
	}while(!(state & Data::EXIT_LOOP));
}

void ListenerSelector::push(const ListenerPtrTp &_rlis, uint _thid){
	d.pfds[d.sz].fd = _rlis->station().descriptor();
	_rlis->setThread(_thid, d.sz);
	d.pfds[d.sz].events = POLLIN;
	d.pfds[d.sz].revents = 0;
	d.pss[d.sz].lisptr = _rlis;
	++d.sz;
}

int ListenerSelector::doReadPipe(){
	enum {BUFSZ = 32, BUFLEN = BUFSZ * sizeof(ulong)};
	uint32	buf[BUFSZ];
	int rv = 0;//no
	int rsz = 0;int j = 0;int maxcnt = (d.cp / BUFSZ) + 1;
	idbg("maxcnt = "<<maxcnt);
	while((++j < maxcnt) && ((rsz = read(d.pipefds[0], buf, BUFLEN)) == BUFLEN)){
		for(int i = 0; i < BUFSZ; ++i){
			idbg("buf["<<i<<"]="<<buf[i]);
			if(buf[i]){
				rv |= Data::FULL_SCAN;	
			}else{ 
				rv |= Data::EXIT_LOOP;
			}
		}
	}
	if(rsz){
		rsz >>= 2;
		for(int i = 0; i < rsz; ++i){	
			idbg("buf["<<i<<"]="<<buf[i]);
			if(buf[i]){
				rv |= Data::FULL_SCAN;
			}else rv = Data::EXIT_LOOP;//must exit
		}
	}
	if(j == maxcnt){
		//dummy read:
		rv = Data::EXIT_LOOP | Data::FULL_SCAN;//scan all filedescriptors for events
		idbg("reading dummy");
		while((rsz = read(d.pipefds[0], buf, BUFSZ)) > 0);
	}
	idbg("readpiperv = "<<rv);
	return rv;
}
//full scan the listeners for io signals and other singnals
int ListenerSelector::doFullScan(){
	uint oldsz = d.sz;
	TimeSpec ts;
	for(uint i = 1; i < d.sz;){
		if(d.pfds[i].revents || d.pss[i].lisptr->signaled(S_RAISE)){
			//TODO: make it use the exec(uint32, TimeSpec)
			switch(d.pss[i].lisptr->execute(0, ts)){
				case BAD:
					idbg("BAD: deleting listener");
					delete d.pss[i].lisptr.release();
					d.pss[i].lisptr = d.pss[d.sz - 1].lisptr;
					d.pfds[i] = d.pfds[d.sz - 1];
					--d.sz;
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
	if(empty() || oldsz == d.cp && d.sz < oldsz) return Data::EXIT_LOOP;
	return 0;
}
//scan only for io signals
int ListenerSelector::doSimpleScan(int _cnt){
	uint oldsz = d.sz;
	TimeSpec ts;
	for(uint i = 1; i < d.sz && _cnt;){
		if(d.pfds[i].revents){
			--_cnt;
			//TODO: make it use the execute(uint32, TimeSpec)
			switch(d.pss[i].lisptr->execute(0, ts)){
				case BAD:
					idbg("BAD: deleting listener");
					delete d.pss[i].lisptr.release();
					d.pss[i].lisptr = d.pss[d.sz - 1].lisptr;
					d.pfds[i] = d.pfds[d.sz - 1];
					--d.sz;
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
	if(empty() || oldsz == d.cp && d.sz < oldsz) return Data::EXIT_LOOP;
	return 0;
}

}//namespace tcp
}//namespace clientserver
