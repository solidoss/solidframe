// testa.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include "system/debug.hpp"
#include "system/thread.hpp"
using namespace std;
using namespace solid;

///\cond 0
struct Runner: public Thread{
	Runner(int _v):v(_v){}
	void run();
private:
	int v;
};
///\endcond
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
