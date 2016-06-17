#include <functional>
#include <boost/function.hpp>
#include <iostream>
#include "system/thread.hpp"
#include "system/specific.hpp"
#include "utility/specificallocator.hpp"
#include "system/debug.hpp"

using namespace std;
using namespace solid;


struct Fun1{
	
	static void operator delete (void *_p, std::size_t _sz){
		vdbg("Delete "<<_p<<" size = "<<_sz);
		free(_p);
	}
	static void* operator new (std::size_t _sz){
		
		void *pv =  malloc(_sz);
		vdbg("Alloc "<<pv<<" size = "<<_sz);
		return pv;
	}
	
	static void * operator new (std::size_t, void * p) throw() {
		vdbg("Alloc");
		return p ;
	}
	
	int		v1;
	char 	v2[32];
	
	Fun1(int _v1, std::string const &_v2):v1(_v1){
		size_t cpsz = 32;
		if(_v2.size() < 32){
			cpsz = _v2.size();
		}
		::memcpy(v2, _v2.data(), cpsz);
	}
	
	int operator()(int _v1, std::string const &_v2){
		vdbg("Operator1");
		return 1;
	}
};


struct Fun2: solid::SpecificObject{
	
	int		v1;
	char 	v2[32];
	
	Fun2(int _v1, std::string const &_v2):v1(_v1){
		size_t cpsz = 32;
		if(_v2.size() < 32){
			cpsz = _v2.size();
		}
		::memcpy(v2, _v2.data(), cpsz);
	}
	
	int operator()(int _v1, std::string const &_v2){
		vdbg("Operator2");
		return 1;
	}
};


struct Fun3{
	
	int		v1;
	char 	v2[32];
	
	Fun3(int _v1, std::string const &_v2):v1(_v1){
		size_t cpsz = 32;
		if(_v2.size() < 32){
			cpsz = _v2.size();
		}
		::memcpy(v2, _v2.data(), cpsz);
	}
	
	int operator()(int _v1, std::string const &_v2){
		vdbg("Operator3");
		return 1;
	}
};

typedef boost::function<int(int, std::string const &)>	FunT;

class Test{
public:
	Test(int _v):value(_v){}
	void set(FunT &_rf){
		auto l = [this](int _v1, std::string const &_v2){return cbk(_v1, _v2);};
		vdbg("sizeof(l) = "<<sizeof(l));
		_rf = l;
	}
private:
	int cbk(int _v1, std::string const &_v2){
		vdbg("Operator4 "<<value);
		return 1;
	}
private:
	int value;
};


int main(int argc, char *argv[]){
	
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any memory_cache");
	Debug::the().initStdErr(false);
	
	solid::Thread::init();
	
	Test t(10);
	
	FunT f;
	
	f = Fun1(1, "ceva");
	
	f(1, "ceva");
	
	f = Fun2(2, "altceva");
	
	f(2, "ceva");
	
	f.assign(Fun3(3, "cleva"), solid::SpecificAllocator<Fun3>());
	
	f(3, "ceva");
	
	t.set(f);
	
	f(4, "test");
	
	return 0;
}

