#include "system/debug.hpp"
#include "system/common.hpp"
#include "utility/functor.hpp"
#include <string>

using namespace solid;

//================================================================================

struct TestAFunctor{
	TestAFunctor(int _v):v(_v){}
	void operator()(){
		idbg("v = "<<v);
	}

	int v;
	int t;
	int c;
};

struct TestBFunctor{
	TestBFunctor(int _v):v(_v){}

	void operator()(const std::string &_rs){
		idbg("v = "<<v<<" string = "<<_rs);
	}
	int operator()(double _x){
		idbg("v = "<<v<<" x = "<<_x);
		return v;
	}
	int v;
	int t;
	int c;
};

struct TestCFunctor{
	TestCFunctor(int _v):v(_v){}

	int operator()(const std::string &_rs){
		idbg("v = "<<v<<" string = "<<_rs);
		return v;
	}
	int v;
	int t;
	int c;
};


struct TestDFunctor{
	TestDFunctor(int _v):v(_v){}
	
	int operator()(int _x, const std::string &_rs){
		idbg("v = "<<v<<" string = "<<_rs<<" x = "<<_x);
		return v;
	}
	int v;
	int t;
	int c;
};

struct TestInt{
	int i;
	TestInt(int _i = 0):i(_i){
		idbg("");
	}
	TestInt(const TestInt &_ri):i(_ri.i){
		idbg("");
	}
	~TestInt(){
		idbg("");
	}
};

struct TestEFunctor{
	TestEFunctor(int _v):v(_v){}
	
	void operator()(const std::string &_rs, int _x){
		idbg("v = "<<v<<" string = "<<_rs<<" x = "<<_x);
	}
	int operator()(const std::string &_rs, int _x, double _d, const TestInt &_ri){
		idbg("v = "<<v<<" string = "<<_rs<<" x = "<<_x<<" d = "<<_d<<" ri = "<<_ri.i);
		return v;
	}
	int v;
	int t;
	int c;
};

int calla(){
	idbg("");
	return 10;
}

int main(int argc, char *argv[]){
#ifdef UDEBUG
	{
		Debug::the().initStdErr();
		Debug::the().levelMask("iwe");
		Debug::the().moduleMask("all");
	}
#endif
	{
		TestAFunctor		af(10);
		
		FunctorReference<>	fs(af);
		
		fs();
	}
	{
		FunctorReference<int>	fs(calla);
		
		int rv = fs();
		idbg("rv = "<<rv);
	}
	{
		TestBFunctor			bf(10);
		FunctorReference<
			void, std::string
		>						fs(bf);
		fs("ceva");
	}
	{
		TestEFunctor					ef(10);
		FunctorReference<
			int,
			const std::string &,
			int, double,
			const TestInt &
		>								fs(ef);
		int rv = fs("ceva", 10, 11.11, 12);
		idbg("rv = "<<rv);
	}
	return 0;
}

