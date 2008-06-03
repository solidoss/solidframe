/* Implementation file test.cpp
	
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

#include <iostream>
#include <signal.h>
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"

#include "core/server.hpp"
#include "echo/echoservice.hpp"
#include "alpha/alphaservice.hpp"
#include "beta/betaservice.hpp"

#include "clientserver/ipc/ipcservice.hpp"
#include "clientserver/tcp/station.hpp"
#include "clientserver/tcp/channel.hpp"
#include "clientserver/udp/station.hpp"

namespace cs = clientserver;
using namespace std;

/*
	The proof of concept test application.
	It instantiate a server, creates some services,
	registers some listeners talkers on those services
	and offers a small CLI.
*/
// prints the CLI help
void printHelp();
// inserts a new talker
int insertTalker(char *_pc, int _len, test::Server &_rts);
// inserts a new connection
int insertConnection(char *_pc, int _len, test::Server &_rts);

int main(int argc, char* argv[]){
	signal(SIGPIPE, SIG_IGN);
	cout<<"Built on SolidGround version "<<SG_MAJOR<<'.'<<SG_MINOR<<'.'<<SG_PATCH<<endl;
	Thread::init();
#ifdef UDEBUG
	{
	string s = "dbg/";
	s+= argv[0]+2;
	initDebug(s.c_str());
	}
#endif
	pdbg("Built on SolidGround version "<<SG_MAJOR<<'.'<<SG_MINOR<<'.'<<SG_PATCH);
	{
		int startport = 1000;
		if(argc >= 2){
			startport = atoi(argv[1]);
		}
		test::Server	ts;
		if(true){// create and register the echo service
			test::Service* psrvc = test::echo::Service::create();
			ts.insertService("echo", psrvc);
			
			{//add a new listener
				int port = startport + 111;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
				if(!ai.empty()){
					if(!ts.insertListener("echo", ai.begin())){
						cout<<"added listener to echo "<<port<<endl;
					}else{
						cout<<"failed adding listener for service echo port "<<port<<endl;
					}
				}else{
					cout<<"failed create address for port "<<port<<" listener not created"<<endl;
				}
			}
			{//add a new talker
				int port = startport + 112;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
				if(!ai.empty()){
					AddrInfoIterator it(ai.begin());
					if(!ts.insertTalker("echo", ai.begin())){
						cout<<"added talker to echo "<<port<<endl;
					}else{
						cout<<"failed creating udp listener station "<<port<<endl;
					}
				}else{
					cout<<"empty addr info - no listener talker"<<endl;
				}
			}
		}
		if(true){//creates and registers a new beta service
			test::Service* psrvc = test::beta::Service::create();
			ts.insertService("beta", psrvc);
			int port = startport + 113;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			if(!ai.empty()){//ands a talker
				if(!ts.insertTalker("beta", ai.begin())){
					cout<<"added talker to beta "<<port<<endl;
				}else{
					cout<<"failed creating udp listener station "<<port<<endl;
				}
			}else{
				cout<<"empty addr info - no listener talker"<<endl;
			}
		}
		if(true){//creates and registers a new alpha service
			test::Service* psrvc = test::alpha::Service::create(ts);
			ts.insertService("alpha", psrvc);
			int port = startport + 114;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
			if(!ai.empty() && !ts.insertListener("alpha", ai.begin())){//adds a listener
				cout<<"added listener for service alpha "<<port<<endl;
			}else{
				cout<<"failed adding listener for service alpha port "<<port<<endl;
			}	
		}
		if(true){//adds the base ipc talker
			int port = startport + 222;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			if(!ai.empty()){
				if(!ts.insertIpcTalker(ai.begin())){
					cout<<"added talker to ipc "<<port<<endl;
				}else{
					cout<<"failed creating udp listener station "<<port<<endl;
				}
			}else{
				cout<<"empty addr info - no listener talker"<<endl;
			}
		}
		char buf[2048];
		int rc = 0;
		// the small CLI loop
		while(true){
			if(rc == -1){
				cout<<"Error: Parsing command line"<<endl;
			}
			if(rc == 1){
				cout<<"Error: executing command"<<endl;
			}
			rc = 0;
			cout<<'>';cin.getline(buf,2048);
			if(!strncasecmp(buf,"quit",4)){
				ts.stop();
				cout<<"signalled to stop"<<endl;
				break;
			}
			if(!strncasecmp(buf,"fetchobj",8)){
				//rc = fetchobj(buf + 8,cin.gcount() - 8,ts,ti);
				continue;
			}
			if(!strncasecmp(buf,"printobj",8)){
				//rc = printobj(buf + 8,cin.gcount() - 8,ts,ti);
				continue;
			}
			if(!strncasecmp(buf,"signalobj",9)){
				//rc = signalobj(buf + 9,cin.gcount() - 9,ts,ti);
				continue;
			}
			if(!strncasecmp(buf,"help",4)){
				printHelp();
				continue;
			}
			if(!strncasecmp(buf,"addtalker",9)){
				rc = insertTalker(buf + 9,cin.gcount() - 9,ts);
				continue;
			}
			if(!strncasecmp(buf,"addconnection", 13)){
				rc = insertConnection(buf + 13, cin.gcount() - 13, ts);
				continue;
			}
			cout<<"Error parsing command line"<<endl;
		}
	}
	Thread::waitAll();
	return 0;
}

void printHelp(){
	cout<<"Server commands:"<<endl;
	cout<<"[quit]:\tStops the server and exits the application"<<endl;
	cout<<"[help]:\tPrint this text"<<endl;
	cout<<"[fetchobj SP service_name SP object_type]: Fetches the list of object_type (int) objects from service named service_name (string)"<<endl;
	cout<<"[printobj]: Prints the list of objects previously fetched"<<endl;
	cout<<"[signalobj SP fetch_list_index SP signal]: Signals an object in the list previuously fetched"<<endl;
}

//>fetchobj servicename type
/*
int fetchobj(char *_pc, int _len,TestServer &_rts,TheInspector &_rti){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	_rti.inspect(_rts, srvname.c_str());
	_rti.print();
	return 0;
}
int printobj(char *_pc, int _len, TestServer &_rts, TheInspector &_rti){
	_rti.print();
	return OK;
}
int signalobj(char *_pc, int _len,TestServer &_rts,TheInspector &_rti){
	if(*_pc != ' ') return -1;
	++_pc;
	
	long idx = strtol(_pc,&_pc,10);
	if(*_pc != ' ') return -1;
	++_pc;
	long sig = strtol(_pc,&_pc,10);
	_rti.signal(_rts,idx,sig);
	return OK;
}
*/

int insertTalker(char *_pc, int _len,test::Server &_rts){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string node;
	while(*_pc != ' ' && *_pc != 0){
		node += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string srv;
	while(*_pc != ' ' && *_pc != 0){
		srv += *_pc;
		++_pc;
	}
	//TODO:
// 	if(_rts.insertTalker(srvname.c_str(), cs::udp::Station::create(), node.c_str(), srv.c_str())){
// 		cout<<"Failed adding talker"<<endl;
// 	}
	return 0;
}
int insertConnection(char *_pc, int _len,test::Server &_rts){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string node;
	while(*_pc != ' ' && *_pc != 0){
		node += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string srv;
	while(*_pc != ' ' && *_pc != 0){
		srv += *_pc;
		++_pc;
	}
	//TODO:
// 	if(_rts.insertConnection(srvname.c_str(), new cs::tcp::Channel, node.c_str(), srv.c_str())){
// 		cout<<"Failed adding connection"<<endl;
// 	}
	return 0;
}
