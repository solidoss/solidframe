/* Implementation file testa.cpp
	
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

#include "utility/workpool.hpp"
#include "system/debug.hpp"
#include <iostream>

using namespace std;

struct MyWorkPoolController;

typedef WorkPool<int, MyWorkPoolController>	MyWorkPool;

struct MyWorkPoolController: WorkPoolControllerBase{
	typedef std::vector<int>	IntVectorT;
	bool createWorker(MyWorkPool &_rwp){
		//_rwp.createSingleWorker()->start();
		_rwp.createMultiWorker(4)->start();
		return true;
	}
	void execute(WorkerBase &, int _i){
		idbg("i = "<<_i);
		Thread::sleep(_i * 10);
	}
	void execute(WorkerBase &, IntVectorT &_rjobvec){
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
	Dbg::instance().levelMask("iew");
	Dbg::instance().moduleMask("any");
	Dbg::instance().initStdErr(true);
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
