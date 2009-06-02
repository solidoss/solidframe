/* Implementation file betatalker.cpp
	
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

#include "core/manager.hpp"
#include "beta/betaservice.hpp"
#include "betatalker.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"
#include <map>
#include <iostream>

using namespace std;

namespace fdt = foundation;

namespace concept{

namespace beta{

struct AddrMap: std::map<Inet4SockAddrPair, pair<uint32, uint32> >{};

Talker::Talker(const char *_node, const char *_srv): pai(NULL), addrmap(*(new AddrMap)){
	if(_node){
		pai = new AddrInfo(_node, _srv);
		//strcpy(bbeg, hellostr);
		sz = BUFSZ;
		state(WRITE);
	}
}
Talker::Talker(const SocketDevice &_rsd): BaseTp(_rsd), pai(NULL), addrmap(*(new AddrMap)){
	state(READ);
}

/*
NOTE: 
* Releasing the connection here and not in a base class destructor because
here we know the exact type of the object - so the service can do other things 
based on the type.
* Also it ensures a proper/safe visiting. Suppose the unregister would have taken 
place in a base destructor. If the visited object is a leaf, one may visit
destroyed data.
NOTE: 
* Visitable data must be destroyed after releasing the connection!!!
*/

Talker::~Talker(){
	concept::Manager &rm = concept::Manager::the();
	rm.service(*this).removeTalker(*this);
	delete pai;
	delete &addrmap;
}
int Talker::execute(ulong _sig, TimeSpec &_tout){
	//if(pai) _tout.set(20);//allways set it if it's not MAXTIMEOUT
	//else _tout.set(0);
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE)){
		if(_sig & fdt::TIMEOUT)
			idbg("talker timeout");
		if(_sig & fdt::ERRDONE)
			idbg("talker error");
		return BAD;
	}
	if(signaled()){
		concept::Manager &rm = concept::Manager::the();
		{
		Mutex::Locker	lock(rm.mutex(*this));
		ulong sm = grabSignalMask(0);
		if(sm & fdt::S_KILL) return BAD;
		}
	}
	int rc = 512 * 1024;
	if(!pai){//receiver
		do{
			switch(state()){
				case READ:
					switch(socketRecvFrom(bbeg, BUFSZ)){
						case BAD: return BAD;
						case OK: break;
						case NOK: state(READ_DONE); return NOK;
					}
				case READ_DONE:{
					AddrMap::iterator it(addrmap.find(socketRecvAddr()));
					pair<uint32, uint32> *pp((pair<uint32, uint32> *)bbeg);
					if(pp->second != socketRecvSize()){
						cout<<"wrong size : should = "<<pp->second<<" is = "<<socketRecvSize()<<endl;
					}
					if(it == addrmap.end()){//add address
						//cout<<"add address "<<(uint32)station().recvAddr()<<" (id = "<<pp->first<<" size = "<<pp->second<<')'<<endl;
						addrmap[socketRecvAddr()] = *pp;
					}else{
						pair<uint32, uint32> *pp((pair<uint32, uint32> *)bbeg);
						if(it->second.first + 1 != pp->first){
							cout<<"missplaced id "<<pp->first<<" size = "<<pp->second<<" - exid = "<<it->second.first<<endl;
							if(pp->first > it->second.first){
								it->second.first = pp->first;
							}
						}else{
							it->second.first = pp->first;
							cout<<"read id "<<pp->first<<" at "<<_tout.seconds()<<':'<<_tout.nanoSeconds()<<endl;
						}
					}
					state(READ);
				}break;
			}
			rc -= socketRecvSize();
		}while(rc > 0);
	}else{//
		switch(state()){
			case WRITE:{
				pair<uint32, uint32> *pp((pair<uint32, uint32> *)bbeg);
				AddrInfoIterator	 it(pai->begin());
				pp->first = id;pp->second = sz; ++id;
				switch(socketSendTo(bbeg, sz, SockAddrPair(it))){
					case BAD: return BAD;
					case OK: state(WRITE); break;
					case NOK: state(WRITE_DONE); return NOK;
				}
			}
			case WRITE_DONE:
				state(WRITE);
				_tout.add(0, 500 * 1000000);//X miliseconds
				//cout<<"write id = "<<(id - 1)<<endl;
				return NOK;
		}
	}
	return OK;
}

int Talker::execute(){
	return BAD;
}


int Talker::accept(fdt::Visitor &_rov){
	//static_cast<TestInspector&>(_roi).inspectTalker(*this);
	return -1;
}
}//namespace echo
}//namespace concept

