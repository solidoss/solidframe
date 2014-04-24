// tests.cpp
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
#include "system/specific.hpp"
#include "system/common.hpp"
#include "speca.hpp"
#include "specb.hpp"

using namespace std;
using namespace solid;

struct SingleTest{
	SingleTest();
	static SingleTest& instance();
	int value;
};

#ifdef HAS_SAFE_STATIC
/*static*/ SingleTest& SingleTest::instance(){
	static SingleTest st;
	return st;
}
#else
SingleTest& test_instance(){
	static SingleTest st;
	return st;
}
void once_test(){
	test_instance();
}
/*static*/ SingleTest& SingleTest::instance(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_test, once);
	return test_instance();
}
#endif

SingleTest::SingleTest():value(0){
	value = 0;
	idbg("before sleep");
	Thread::sleep(3000);
	idbg("after sleep");
	value = 1;
}


///\cond 0
struct Runner: public Thread{
	Runner(int _v):v(_v){}
	void run();
private:
	int v;
};

typedef Cacheable<vector<int>, 2> CacheableVecT;

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
///\endcond

void Runner::run(){
	idbg("runner::run");
	int val = SingleTest::instance().value;
	idbg("Simple test value runner = "<<val);
	Thread::sleep(100);
	Specific::prepareThread();
	idbg("Uncaching some object");
	testa();
	testb();
	CacheableVecT *pc1 = Specific::uncache<CacheableVecT>();
	idbg("uncahed vector (should be same as the last cached vector) "<<(void*)pc1);
	cout<<"uncahed vector (should be same as the last cached vector) "<<(void*)pc1<<endl;
	CacheableVecT *pc2 = Specific::uncache<CacheableVecT>();
	idbg(pc2);
	CacheableVecT *pc3 = Specific::uncache<CacheableVecT>();
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
		idbg("sizeToId("<<(1<<i)<<") = "<<Specific::sizeToIndex((1<<i))<<" "<<Specific::indexToCapacity(Specific::sizeToIndex((1<<i))));
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
	pc1 = Specific::uncache<CacheableVecT>();

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
	cout<<"Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH<<endl;
#ifdef UDEBUG
	{
	//initDebug(s.c_str());
		Debug::the().initStdErr();
		Debug::the().levelMask("iwe");
		Debug::the().moduleMask("all");
	}
#endif
	Thread::init();
	idbg("current thread "<<(void*)&Thread::current());
	const unsigned specid(Thread::specificId());
	Thread::specific(specid,  reinterpret_cast<void*>(123456));
	idbg("saved specific value = "<<(solid::ulong)Thread::specific(specid));
	Runner *pth = new Runner(2);
	pth->start(true);
	idbg("before reading sleep");
	Thread::sleep(1000);
	idbg("before reading the simple test value");
	int val = SingleTest::instance().value;
	idbg("Simple test value = "<<val);
	idbg("before wait");
	Thread::waitAll();
	return 0;
}

