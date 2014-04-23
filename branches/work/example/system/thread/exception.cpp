#include "system/exception.hpp"
#include <iostream>
#include "system/common.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"

using namespace std;
using namespace solid;


struct TwoInts{
	TwoInts(int _a, int _b):a(_a), b(_b){}
	int a;
	int b;
};

std::ostream& operator<<(std::ostream &_ros, const TwoInts &_r2i){
	_ros<<'('<<_r2i.a<<','<<_r2i.b<<')';
	return _ros;
}

std::ostream& operator<<(std::ostream &_ros, const Tuple<int, TwoInts, const char*> &_r2i){
	_ros<<'['<<_r2i.o0<<' '<<_r2i.o1<<' '<<_r2i.o2<<']';
	return _ros;
}
int main(){
#ifdef UDEBUG
	Debug::the().initStdErr();
#endif
	try{
		THROW_EXCEPTION("simple exception ever");
		//create_exeption("best exception ever", __FILE__, __LINE__);
		//throw Exception("best exception ever", __FILE__, __LINE__);
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	
	try{
		THROW_EXCEPTION_EX("3-tuple exception", solid::make_tuple(2,TwoInts(9,10),static_cast<const char*>("a string")));
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	try{
		THROW_EXCEPTION_EX("Last system error.", last_system_error());
	}catch(exception &e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	return 0;
}

