#include "system/memorycache.hpp"
#include "system/specific.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include <cstring>
#include <vector>
#include <iostream>
using namespace std;
using namespace solid;

namespace {
MemoryCache	mc(0, 512 + 256);
bool		usemc = true;
}

//#define USE_MEMORYCACHE

#ifdef USE_MEMORYCACHE

struct BaseObject{
	static void operator delete (void *_p, std::size_t _sz){
		if(!usemc){
			free(_p);
		}else{
			mc.free(_p, _sz);
		}
	}
	static void* operator new (std::size_t _sz){
		if(!usemc){
			return malloc(_sz);
		}else{
			return mc.allocate(_sz);
		}
	}
	virtual ~BaseObject(){}
};

#define MEMCACHE mc

#else

struct BaseObject: SpecificObject{
	virtual ~BaseObject(){}
};
#define MEMCACHE Specific::the()
#endif

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

template <>
struct TestObject<12>: BaseObject{
	uint64	v1;
	uint32	v2;
	TestObject(uint64 _v = 0):v1(_v),v2(_v){}
};

template <uint16 Sz>
struct TestObject: BaseObject{
	char buf[Sz];
};


int main(int argc, char *argv[]){
	if(argc >= 2){
		usemc = false;
	}
	solid::Thread::init();
	solid::Debug::the().initStdErr(false);
	solid::Debug::the().moduleMask("all");
	solid::Debug::the().levelMask("iew");
	
	std::vector<BaseObject* > vec;
	
	const size_t step = 3000;
	const size_t repeatcnt = 100;
	const size_t cp = repeatcnt * step;
	const size_t fullrepeatcnt = 10;
	vec.reserve(cp * 3);
	
	size_t rescnt = 0;
	if(usemc){
		rescnt = MEMCACHE.reserve(sizeof(TestObject<4>),  cp);
		rescnt = MEMCACHE.reserve(sizeof(TestObject<8>),  cp);
		rescnt = MEMCACHE.reserve(sizeof(TestObject<12>), cp);
		rescnt = MEMCACHE.reserve(sizeof(TestObject<32>), cp);
	}
	idbg("Reserved "<<rescnt<<" items");
	cout<<"Reserved "<<rescnt<<" items"<<endl;
	
	for(size_t k = 0; k < fullrepeatcnt; ++k){
		size_t crtcp = 0;
		for(size_t i = 0; i < repeatcnt; ++i){
			crtcp += step;
			idbg("Allocate "<<crtcp<<" items");
			for(size_t j = 0; j < crtcp; ++j){
				vec.push_back(new TestObject<4>(j));
				vec.push_back(new TestObject<8>(j));
				vec.push_back(new TestObject<12>(j));
				vec.push_back(new TestObject<32>);
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
	}
	return 0;
}

