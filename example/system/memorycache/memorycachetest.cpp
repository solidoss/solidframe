#include "system/memorycache.hpp"
#include "system/debug.hpp"
#include <cstring>
#include <vector>
#include <iostream>
using namespace std;
using namespace solid;

namespace {
MemoryCache	mc(0, 512 + 256);
bool		usemc = true;
}

struct BaseObject{
	static void operator delete (void *_p, std::size_t _sz){
		if(usemc){
			mc.free(_p, _sz);
		}else{
			free(_p);
		}
	}
	static void* operator new (std::size_t _sz){
		if(usemc){
			return mc.allocate(_sz);
		}else{
			return malloc(_sz);
		}
	}
	virtual ~BaseObject(){}
};

template <uint16 Sz>
struct TestObject;

template <>
struct TestObject<4>: BaseObject{
	uint32	v;
	TestObject(uint32 _v = 0):v(_v){}
};

template <>
struct TestObject<8>: BaseObject{
	uint64	v;
	TestObject(uint64 _v = 0):v(_v){}
};


template <uint16 Sz>
struct TestObject: BaseObject{
	char buf[Sz];
};


int main(int argc, char *argv[]){
	if(argc >= 2){
		usemc = false;
	}
	solid::Debug::the().initStdErr(false);
	solid::Debug::the().moduleMask("all");
	solid::Debug::the().levelMask("iew");
	
// 	TestObject<4> *p4 = new TestObject<4>;
// 	memset(p4->buf, 0, 4);
// 	delete p4;
	std::vector<BaseObject* > vec;
	
	size_t step = 3000;
	size_t repeatcnt = 100;
	size_t cp = repeatcnt * step;
	vec.reserve(cp * 3);
	
	size_t rescnt = 0;
	//rescnt = mc.reserve(sizeof(TestObject<4>),  cp);
	//rescnt = mc.reserve(sizeof(TestObject<8>),  cp);
	//rescnt = mc.reserve(sizeof(TestObject<16>), cp);
	idbg("Reserved "<<rescnt<<" items");
	cout<<"Reserved "<<rescnt<<" items"<<endl;
	
	size_t crtcp = 0;
	for(size_t i = 0; i < repeatcnt; ++i){
		crtcp += step;
		idbg("Allocate "<<crtcp<<" items");
		for(size_t j = 0; j < crtcp; ++j){
			vec.push_back(new TestObject<4>(j));
			vec.push_back(new TestObject<8>(j));
			vec.push_back(new TestObject<16>);
		}
		idbg("+++++++++++++++++++++++++++++");
		mc.print(4);
		idbg("-----------------------------");
		idbg("delete "<<vec.size()<<" items");
		for(auto it = vec.begin(); it != vec.end(); ++it){
			delete *it;
		}
		idbg("Done delete");
		idbg("+++++++++++++++++++++++++++++");
		mc.print(4);
		idbg("-----------------------------");
		vec.clear();
	}
	return 0;
}

