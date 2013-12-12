#include <iostream>
#include <vector>
#define USE_BOOST_FNC
#ifndef USE_BOOST_FNC
#include <functional>
#define FNCNS std
#else
#include "boost/function.hpp"
#define FNCNS boost
#endif

#undef UDEBUG
#include "system/debug.hpp"
#include <cstdio>

using namespace std;
using namespace solid;

void * operator new(std::size_t size) throw(std::bad_alloc) {
	// we are required to return non-null
	void * mem = std::malloc(size == 0 ? 1 : size);
	if(mem == 0) {
		throw std::bad_alloc();
	}
	fprintf(stderr, "new %d %x\n", size, mem);
	return mem;
}

void operator delete(void * mem) throw() {
	std::free(mem);
	fprintf(stderr, "del %x\n", mem);
}

struct Functor{
	Functor(int _m = 0):mul(_m){
		idbg("");
	}
	~Functor(){
		idbg("");
	}
	Functor(const Functor &_fnc):mul(_fnc.mul){
		idbg("");
	}
	Functor& operator=(const Functor &_fnc){
		idbg("");
		mul = _fnc.mul;
		return *this;
	}
	int operator()(int _v){
		return mul * _v;
	}
	
	int mul;
};


int main(){
	string dbgout;
	//Debug::the().levelMask("view");
	//Debug::the().moduleMask("any");
	//Debug::the().initStdErr(false);
	{
		vector<FNCNS::function<int(int)> > fncvec;
		FNCNS::function<int(int)>	fnc = Functor(10);
// 		FNCNS::function<int(int)>	fnc2;
// 		
// 		fnc2 = fnc;
		fnc = Functor(11);
		cout<<"val = "<<fnc(10)<<endl;
		
// 		fncvec.push_back(FNCNS::function<int(int)>());
// 		fncvec.push_back(FNCNS::function<int(int)>());
// 		fncvec[0] = Functor(10);
// 		fncvec[1] = Functor(20);
// 		
// 		cout<<"val = "<<fncvec[0](3)<<endl;
// 		cout<<"val = "<<fncvec[1](4)<<endl;
	}
	return 0;
};
