#include "dltestb.hpp"
#include "dltesta.hpp"
#include "system/debug.hpp"

using namespace std;

extern "C"{
Test* createB(int _b){
	return new TestB(_b * TestA::instance().get());
}

}

TestB::TestB(int _b):b(_b){
}
TestB::~TestB(){
}
void TestB::set(int _b){
	b = _b * TestA::instance().get();
}
int TestB::get()const{
	return b;
}
void TestB::print(){
	idbg("testb = "<<b<<" testa ptr = "<<(void*)&TestA::instance());
}
