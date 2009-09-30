#include "system/debug.hpp"
#include "utility/functorstub.hpp"

struct TestAFunctor{
	TestAFunctor(int _v):v(_v){
		idbg("");
	}
	TestAFunctor(const TestAFunctor &_rv){
		idbg("");
		*this = _rv;
	}
	~TestAFunctor(){
		idbg("");
	}
	int operator()(){
		idbg("v = "<<v);
		return v;
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

struct TestEFunctor{
	TestEFunctor(int _v):v(_v){}
	
	void operator()(const std::string &_rs, int _x){
		idbg("v = "<<v<<" string = "<<_rs<<" x = "<<_x);
	}
	int v;
	int t;
	int c;
};

struct TestFFunctor{
	TestFFunctor(int _v):v(_v){}
	
	void operator()(const std::string &_rs, int _x, const char *_str){
		idbg("v = "<<v<<" string = "<<_rs<<" x = "<<_x<<" str = "<<_str);
	}
	int operator()(const std::string &_rs, int _x, const char *_str, char _c, short _s, ushort _us, ulong _ul){
		idbg("v = "<<v<<" rs = "<<_rs<<" x = "<<_x<<" _str = "<<_str<<" c = "<<_c<<" s = "<<_s<<" us = "<<_us<<" ul = "<<_ul);
		return v;
	}
	int v;
	int t;
	int c;
};


int main(int argc, char *argv[]){
	{
		Dbg::instance().initStdErr();
		Dbg::instance().levelMask("iwe");
		Dbg::instance().moduleMask("all");
	}
	//FunctorStub<NoType>				fnc0;
	FunctorStub<3 * sizeof(int), int>							fnca;
	FunctorStub<4, void, const std::string&>					fncb;
	FunctorStub<4, int, const std::string&>						fncc;
	FunctorStub<4, int, int, const std::string&>				fncd;
	FunctorStub<4, void, const std::string&, int>				fnce;
	FunctorStub<4, void, const std::string&, int, const char *>	fncf;
	FunctorStub<3 * sizeof(int), int, const std::string&, int, const char*, char, short, ushort, ulong> fncf2;
	fnca = TestAFunctor(11);
	fncb = TestBFunctor(12);
	fncc = TestCFunctor(13);
	fncd = TestDFunctor(14);
	fnce = TestEFunctor(15);
	fncf = TestFFunctor(16);
	fncf2 = TestFFunctor(17);
	//fncc = TestAFunctor(14);
	int v;
	v = fnca();
	idbg("v = "<<v);
	fncb("ceva");
	v = fncc("ceva");
	idbg("v = "<<v);
	v = fncd(2, "altceva");
	idbg("v = "<<v);
	fnce("altceva2", 3);
	fncf("altceva2", 3, "super");
	v = fncf2("alt", 4, "mega", 'c', 1, 2, 3);
	idbg("v = "<<v);
	return 0;
}

