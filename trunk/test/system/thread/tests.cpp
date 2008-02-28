/* Implementation file tests.cpp
	
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
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/specific.hpp"
#include "system/common.hpp"
#include "speca.h"
#include "specb.h"

using namespace std;


struct Runner: public Thread{
	Runner(int _v):v(_v){}
	void run();
private:
	int v;
};

typedef Cacheable<vector<int>, 2> CacheableVecTp;

struct A: SpecificObject{
	virtual void print(){
		idbg("h");
	}
};

struct B: A{
	B(int _d):d(_d){}
	virtual void print(){
		idbg("h:"<<d);
	}
	int d;
};

struct C: A{
	C(int64 _d):d(_d){}
	virtual void print(){
		idbg("h:"<<d);
	}
	int64	d;
};


void Runner::run(){
	idbg("runner::run");
	Specific::prepareThread();
	idbg("Uncaching some object");
	testa();
	testb();
	CacheableVecTp *pc1 = Specific::uncache<CacheableVecTp>();
	idbg("uncahed vector (should be same as the last cached vector) "<<(void*)pc1);
	cout<<"uncahed vector (should be same as the last cached vector) "<<(void*)pc1<<endl;
	CacheableVecTp *pc2 = Specific::uncache<CacheableVecTp>();
	idbg(pc2);
	CacheableVecTp *pc3 = Specific::uncache<CacheableVecTp>();
	idbg(pc3);
	
	idbg("alloc two specific objects");
	A	*pa1 = new B(10);
	idbg(pa1);
	A	*pa2 = new C(20);
	idbg(pa2);
	pa1->print();
	pa2->print();
	idbg("alloc two buffers");
	void	*b1,*b2,*b3;
	size_t  s1 = 10, s2 = 2000, s3 = 4000;
	
	for(int i = 0; i < 20; ++i){
		idbg("sizeToId("<<(1<<i)<<") = "<<Specific::sizeToId((1<<i))<<" "<<Specific::idToCapacity(Specific::sizeToId((1<<i))));
	}
	
/*	b1 = Specific::popBuffer(Specific::s1);idbg(b1<<" "<<s1);
	b2 = Specific::bufferAlloc(s2);idbg(b2<<" "<<s2);
	b3 = Specific::bufferAlloc(s3);idbg(b3<<" "<<s3);*/
	
	for(int i = 0; i < v; ++i){
		idbg(i);
		Thread::sleep(10);
	}
	Specific::cache(pc1);
	Specific::cache(pc2);
	Specific::cache(pc3);
	pc1 = Specific::uncache<CacheableVecTp>();
	
	idbg("test specific object");
	delete pa1;
	delete pa2;
	pa1 = new B(10);
	idbg(pa1);
	pa2 = new C(20);
	idbg(pa2);
	
	idbg("test buffer");
/*	Specific::bufferFree(b1,s1);
	Specific::bufferFree(b2,s2);
	Specific::bufferFree(b3,s3);*/
	
// 	b1 = Specific::bufferAlloc(s1);idbg(b1<<" "<<s1);
// 	b2 = Specific::bufferAlloc(s2);idbg(b2<<" "<<s2);
// 	b3 = Specific::bufferAlloc(s3);idbg(b3<<" "<<s3);
	delete pa1;
	delete pa2;
	Specific::cache(pc1);
}

int main(int argc, char *argv[]){
	{
	string s = "dbg/";
	s+= argv[0]+2;
	//initDebug(s.c_str());
	}
	Thread::init();
	Runner *pth = new Runner(2);
	pth->start(true);
	idbg("before wait");
	Thread::waitAll();
	return 0;
}

