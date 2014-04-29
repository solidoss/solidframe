// testa.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "utility/workpool.hpp"
#include "system/debug.hpp"
#include <iostream>

using namespace std;
using namespace solid;

struct MyWorkPoolController;

typedef WorkPool<int, MyWorkPoolController>	MyWorkPool;

struct MyWorkPoolController: WorkPoolControllerBase{
	typedef std::vector<int>	IntVectorT;
	bool createWorker(MyWorkPool &_rwp, ushort _wkrcnt){
		//_rwp.createSingleWorker()->start();
		_rwp.createMultiWorker(4)->start();
		return true;
	}
	void execute(WorkPoolBase &_rwp, WorkerBase &, int _i){
		idbg("i = "<<_i);
		Thread::sleep(_i * 10);
	}
	void execute(WorkPoolBase &_rwp, WorkerBase &, IntVectorT &_rjobvec){
		for(IntVectorT::const_iterator it(_rjobvec.begin()); it != _rjobvec.end(); ++it){
			idbg("it = "<<*it);
			Thread::sleep(*it * 10);
		}
	}
};


int main(int argc, char *argv[]){
	Thread::init();
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask("iew");
	Debug::the().moduleMask("any");
	Debug::the().initStdErr(true);
	}
#endif
	MyWorkPool	mwp;
	mwp.start(2);
	
	for(int i(0); i < 100; ++i){
		mwp.push(i);
	}
	mwp.stop(true);
	Thread::waitAll();
	return 0;
}
