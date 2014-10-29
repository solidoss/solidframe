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
MemoryCache	mc(0)/*(0, 512 + 256)*/;
enum{
	MallocE = 0,
	CacheE,
	SpecificE,
	UnknownE,
} choice(UnknownE);
}

struct Base{
	virtual ~Base(){}
};

struct CacheBase: Base{
	static void operator delete (void *_p, std::size_t _sz){
		mc.free(_p, _sz);
	}
	static void* operator new (std::size_t _sz){
		return mc.allocate(_sz);
	}
	
};

struct SpecificBase: SpecificObject, Base{
	
};

template <uint16 Sz, class B>
struct Test;

template <class B>
struct Test<4, B>: B{
	uint32	v;
	Test(uint32 _v = 0):v(_v){}
};

template <class B>
struct Test<8, B>: B{
	uint64	v;
	Test(uint64 _v = 0):v(_v){}
};

template <class B>
struct Test<12, B>: B{
	uint64	v1;
	uint32	v2;
	Test(uint64 _v = 0):v1(_v),v2(_v){}
};

template <uint16 Sz, class B>
struct Test: B{
	char buf[Sz];
};

static void push_cache(std::vector<Base* > &_rvec, const size_t _cp){
	for(size_t i = 0; i < _cp; ++i){
		_rvec.push_back(new Test<4,  CacheBase>(i));
		_rvec.push_back(new Test<8,  CacheBase>(i));
		_rvec.push_back(new Test<12, CacheBase>(i));
		_rvec.push_back(new Test<32, CacheBase>);
	}
}

static void push_specific(std::vector<Base* > &_rvec, const size_t _cp){
	for(size_t i = 0; i < _cp; ++i){
		_rvec.push_back(new Test<4,  SpecificBase>(i));
		_rvec.push_back(new Test<8,  SpecificBase>(i));
		_rvec.push_back(new Test<12, SpecificBase>(i));
		_rvec.push_back(new Test<32, SpecificBase>);
	}
}

static void push_malloc(std::vector<Base* > &_rvec, const size_t _cp){
	for(size_t i = 0; i < _cp; ++i){
		_rvec.push_back(new Test<4,  Base>(i));
		_rvec.push_back(new Test<8,  Base>(i));
		_rvec.push_back(new Test<12, Base>(i));
		_rvec.push_back(new Test<32, Base>);
	}
}

typedef void (*FncT)(std::vector<Base* > &, const size_t);

const FncT	pushfnctbl[3] = {push_malloc, push_cache, push_specific};

int main(int argc, char *argv[]){
	if(argc == 2){
		if(*argv[1] == 'm' || *argv[1] == 'M'){
			choice = MallocE;
		}else if(*argv[1] == 'c' || *argv[1] == 'C'){
			choice = CacheE;
		}else if(*argv[1] == 's' || *argv[1] == 'S'){
			choice = SpecificE;
		}
	}
	if(argc != 2 || choice == UnknownE){
		cout<<"Usage: "<<endl;
		cout<<argv[0]<<" m"<<endl;
		cout<<"\tUse system malloc/free"<<endl;
		cout<<argv[0]<<" c"<<endl;
		cout<<"\tUse MemoryCache directly"<<endl;
		cout<<argv[0]<<" s"<<endl;
		cout<<"\tUse specific MemoryCache"<<endl;
		return 0;
	}
	
	solid::Thread::init();
	solid::Specific::the().configure();
	
	solid::Debug::the().initStdErr(false);
	solid::Debug::the().moduleMask("all");
	solid::Debug::the().levelMask("iew");
	
	std::vector<Base* > vec;
	
	const size_t step = 3000;
	const size_t repeatcnt = 100;
	const size_t cp = repeatcnt * step;
	const size_t fullrepeatcnt = 10;
	vec.reserve(cp * 3);
	
	size_t rescnt = 0;
	if(choice == CacheE){
		rescnt = mc.reserve(sizeof(Test<4,  CacheBase>),  cp);
		rescnt = mc.reserve(sizeof(Test<8,  CacheBase>),  cp);
		rescnt = mc.reserve(sizeof(Test<12, CacheBase>), cp);
		rescnt = mc.reserve(sizeof(Test<32, CacheBase>), cp);
	}else if(choice == SpecificE){
		rescnt = Specific::the().reserve(sizeof(Test<4,  SpecificBase>),  cp);
		rescnt = Specific::the().reserve(sizeof(Test<8,  SpecificBase>),  cp);
		rescnt = Specific::the().reserve(sizeof(Test<12, SpecificBase>), cp);
		rescnt = Specific::the().reserve(sizeof(Test<32, SpecificBase>), cp);
	}
	idbg("Reserved "<<rescnt<<" items");
	cout<<"Reserved "<<rescnt<<" items"<<endl;
	
	for(size_t k = 0; k < fullrepeatcnt; ++k){
		size_t crtcp = 0;
		for(size_t i = 0; i < repeatcnt; ++i){
			crtcp += step;
			idbg("Allocate "<<crtcp<<" items");

			(pushfnctbl[choice])(vec, crtcp);
			
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

