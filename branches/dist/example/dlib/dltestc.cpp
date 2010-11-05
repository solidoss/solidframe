#include "dltestc.hpp"
#include "dltesta.hpp"
#include "system/debug.hpp"

using namespace std;

extern "C"{
Test* createC(int _c){
	return new TestC(_c + TestA::instance().get());
}
}

TestC::TestC(int _c):c(_c){
}
TestC::~TestC(){
}
void TestC::set(int _c){
	c = _c + TestA::instance().get();
}
int TestC::get()const{
	return c;
}
void TestC::print(){
	idbg("testc = "<<c<<" testa ptr = "<<(void*)&TestA::instance());
}
