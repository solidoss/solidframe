/* Implementation file testa.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "../debug.h"
#include "threadpp.h"
using namespace std;


struct Runner: public Thread{
	Runner(int _v):v(_v){}
	void run();
private:
	int v;
};

void Runner::run(){
	idbg("runner::run");
	for(int i = 0; i < v; ++i){
		idbg(i);
		Thread::sleep(1000);
	}
}

int main(int argc, char *argv[]){
	{
	string s = "dbg/";
	s+= argv[0]+2;
	//initDebug(s.c_str());
	}
	Thread::init();
	for(int i = 10; i < 100; i+=10){
		idbg("create "<<i);
		Runner *pth = new Runner(i);
		pth->start(true);
	}
	//Thread::sleep(100);
	idbg("before wait");
	Thread::waitAll();
	return 0;
}
