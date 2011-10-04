#include "dltesta.hpp"
/*static*/ TestA& TestA::instance(){
	static TestA a;
	return a;
}
TestA::TestA():a(2){}
void TestA::set(int _a){
	a = _a;
}
int TestA::get()const{
	return a;
}