#include "system/memorycache.hpp"
using namespace std;
using namespace solid;

MemoryCache mc;


struct BaseObject{
	static void operator delete (void *_p, std::size_t _sz){
		mc.free(_p, _sz);
	}
	static void* operator new (std::size_t _sz){
		return mc.allocate(_sz);
	}
};


template <uint16 Sz>
struct TestObject: BaseObject{
	char buf[Sz];
};


int main(int argc, char *argv[]){
	TestObject<4> *p4 = new TestObject<4>;
	delete p4;
	return 0;
}

