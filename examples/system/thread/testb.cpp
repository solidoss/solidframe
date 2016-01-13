// testb.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#define USETHREADS
#include <iostream>
#include "system/thread.hpp"
#include "system/debug.hpp"
#include <string>

using namespace std;
using namespace solid;

//TODO: Update code

///\cond 0
struct MyThreadBase: public ThreadBase{
	static void initSpecific(){
		ThreadBase::specific(new string("gigi"));
		ThreadBase::specific(new int(10));
		ThreadBase::specific((void*)NULL);
	}
	static void destroySpecific(){
		delete reinterpret_cast<string*>(ThreadBase::specific(0));
		delete reinterpret_cast<int*>(ThreadBase::specific(1));
		delete reinterpret_cast<string*>(ThreadBase::specific(2));
	}
	static string &getString(){
		return *reinterpret_cast<string*>(ThreadBase::specific(0));
	}
	static int& getInt(){
		return *reinterpret_cast<int*>(ThreadBase::specific(1));
	}
	static string &getString2(){
		return *reinterpret_cast<string*>(ThreadBase::specific(2));
	}
	static void setString2(const char *str){
		string *ps = new string("str");
		INF1("setting string s = %p",ps);
		ThreadBase::specific(2, ps);
	}
};

struct Runner: public MyThreadBase{
	typedef int RunParamT;
	void run(RunParamT _v);
};
///\endcond

void Runner::run(RunParamT _v){
	MyThreadBase::setString2("theone");
	cout<<"Threadstatic string: "<<MyThreadBase::getString()<<endl;
	for(MyThreadBase::getInt() = 0; MyThreadBase::getInt() < _v; ++MyThreadBase::getInt()){
		INF1("%i",MyThreadBase::getInt());
		ThreadBase::sleep(100);
	}
	INF1("second string %s",MyThreadBase::getString2().c_str());
}

int main(int argc, char *argv[]){
	{
	string s = "dbg/";
	s+= argv[0]+2;
	initDebug(s.c_str());
	}
	ThreadBase::init();
	for(int i = 10; i < 100; i+=10)
		Thread<Runner>::create(i);
	ThreadBase::waitAll();
	return 0;
}
