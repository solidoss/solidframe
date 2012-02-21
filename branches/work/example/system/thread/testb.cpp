/* Implementation file testb.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#define USETHREADS
#include <iostream>
#include "threadpp.h"
#include "../debug.h"
#include <string>

using namespace std;
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
