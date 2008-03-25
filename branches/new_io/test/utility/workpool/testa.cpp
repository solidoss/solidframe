/* Implementation file testa.cpp
	
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

#include "workpoolpp.h"
#include <iostream>

using namespace std;


class MyWorkPool: public WorkPool<int>{
public:
	void run(Worker &);
protected:
	int createWorkers(uint);
};

void MyWorkPool::run(Worker &_wk){
	int k;
	while(!pop(_wk.wid(),k)){
		idbg(_wk.wid()<<" is processing "<<k);
		Thread::sleep(1000);
	}
}
int MyWorkPool::createWorkers(uint _cnt){
	//typedef GenericWorker<Worker, MyWorkPool> MyWorkerTp;
	for(uint i = 0; i < _cnt; ++i){
		Worker *pw = createWorker<Worker>(*this);//new MyWorkerTp(*this);
		pw->start(true);//wait for start
	}
	return _cnt;
}


int main(int argc, char *argv[]){
	{
	string s = "dbg/";
	s+= argv[0]+2;
	//initDebug(s.c_str());
	}
	Thread::init();
	MyWorkPool mwp;
	idbg("before start");
	mwp.start(5);
	idbg("after start");
	for(int i = 0; i < 1000; ++i){
		mwp.push(i);
		if(!(i % 10)) Thread::sleep(500);
	}
	idbg("before stop");
	mwp.stop();
	Thread::waitAll();
	return 0;
}
