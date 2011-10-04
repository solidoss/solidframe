#include "system/exception.hpp"
#include <iostream>
#include "system/common.hpp"

using namespace std;



struct TwoInts{
	TwoInts(int _a, int _b):a(_a), b(_b){}
	int a;
	int b;
};

std::ostream& operator<<(std::ostream &_ros, const TwoInts &_r2i){
	_ros<<'('<<_r2i.a<<','<<_r2i.b<<')';
	return _ros;
}

int main(){
#ifdef UDEBUG
	Dbg::instance().initStdErr();
#endif
	try{
		THROW_EXCEPTION("simple exception ever");
		//create_exeption("best exception ever", __FILE__, __LINE__);
		//throw Exception("best exception ever", __FILE__, __LINE__);
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	
	try{
		THROW_EXCEPTION_EX("3-tuple exception", make_tuple(2,TwoInts(9,10),static_cast<const char*>("a string")));
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	
	return 0;
}

