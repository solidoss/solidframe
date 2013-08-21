#include "system/debug.hpp"
#include "system/common.hpp"
#include "utility/functorstub.hpp"
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
#if 0

template <
	class R = void,
	class P1 = void, class P2 = void,
	class P3 = void, class P4 = void
>
class FunctorStub;

template <typename T>
struct FunctorRef{
	T	&ref;
	FunctorRef(T &_ref):ref(_ref){}
};

template <
	class R
>
class FunctorStub<R, void, void, void, void>{
	explicit FunctorStub(const FunctorStub&);
	typedef FunctorStub<R, void, void, void, void>	FunctorStubT;
	typedef R (*CallFncT)(void*);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRef<int>)
	};
	template <typename T>
	static R call(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref();
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorStub(F &_f){
		/*FunctorRef<F>	*pfr = */new(d) FunctorRef<F>(_f);
		pcallfnc = &call<FunctorRef<F> >;
		pdelfnc = &del<FunctorRef<F> >;
	}
	~FunctorStub(){
		(*pdelfnc)(d);
	}
	
	R operator()(){
		return (*pcallfnc)(d);
	}
};


template <
	class R, class P1
>
class FunctorStub<R, P1, void, void, void>{
	explicit FunctorStub(const FunctorStub&);
	typedef FunctorStub<R, P1, void, void, void>	FunctorStubT;
	typedef R (*CallFncT)(void*, P1);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRef<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorStub(F &_f){
		/*FunctorRef<F>	*pfr = */new(d) FunctorRef<F>(_f);
		pcallfnc = &call<FunctorRef<F> >;
		pdelfnc = &del<FunctorRef<F> >;
	}
	~FunctorStub(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1){
		return (*pcallfnc)(d, _p1);
	}
};

template <
	class R, class P1, class P2
>
class FunctorStub<R, P1, P2, void, void>{
	explicit FunctorStub(const FunctorStub&);
	typedef FunctorStub<R, P1, P2, void, void>	FunctorStubT;
	typedef R (*CallFncT)(void*, P1, P2);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRef<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorStub(F &_f){
		/*FunctorRef<F>	*pfr = */new(d) FunctorRef<F>(_f);
		pcallfnc = &call<FunctorRef<F> >;
		pdelfnc = &del<FunctorRef<F> >;
	}
	~FunctorStub(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2){
		return (*pcallfnc)(d, _p1, _p2);
	}
};

template <
	class R, class P1, class P2, class P3
>
class FunctorStub<R, P1, P2, P3, void>{
	explicit FunctorStub(const FunctorStub&);
	typedef FunctorStub<R, P1, P2, P3, void>	FunctorStubT;
	typedef R (*CallFncT)(void*, P1, P2, P3);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRef<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2, P3 _p3){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2, _p3);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorStub(F &_f){
		/*FunctorRef<F>	*pfr = */new(d) FunctorRef<F>(_f);
		pcallfnc = &call<FunctorRef<F> >;
		pdelfnc = &del<FunctorRef<F> >;
	}
	~FunctorStub(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2, P3 _p3){
		return (*pcallfnc)(d, _p1, _p2, _p3);
	}
};

template <
	class R, class P1, class P2, class P3, class P4
>
class FunctorStub{
	explicit FunctorStub(const FunctorStub&);
	typedef FunctorStub<R, P1, P2, P3, P4>	FunctorStubT;
	typedef R (*CallFncT)(void*, P1, P2, P3, P4);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRef<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2, P3 _p3, P4 _p4){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2, _p3, _p4);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorStub(F &_f){
		/*FunctorRef<F>	*pfr = */new(d) FunctorRef<F>(_f);
		pcallfnc = &call<FunctorRef<F> >;
		pdelfnc = &del<FunctorRef<F> >;
	}
	~FunctorStub(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4){
		return (*pcallfnc)(d, _p1, _p2, _p3, _p4);
	}
};
#endif

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
		TestAFunctor	af(10);
		
		FunctorStub<>	fs(af);
		
		fs();
	}
	{
		FunctorStub<int>	fs(calla);
		
		int rv = fs();
		idbg("rv = "<<rv);
	}
	{
		TestBFunctor					bf(10);
		FunctorStub<void, std::string>	fs(bf);
		fs("ceva");
	}
	{
		TestEFunctor					ef(10);
		FunctorStub<
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

