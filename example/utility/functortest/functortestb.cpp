#include "system/debug.hpp"
#include "system/common.hpp"

struct NoType{};

//================================================================================

//================================================================================
template <
	unsigned DSZ,
	class R,
	class P1, class P2, class P3
>
class FunctorBase;

struct CallerBase{
	virtual ~CallerBase(){}
	virtual void destroy(void *_p){}
	virtual size_t size()const{return 0;}
};

template <unsigned DSZ, class R>
class FunctorBase<DSZ, R, NoType, NoType, NoType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv){
			return ref(_pv)();
		}
		void destroy(void *_p){
			if(sizeof(T) <= DSZ)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
};

template <unsigned DSZ, class R, class P1>
class FunctorBase<DSZ, R, P1, NoType, NoType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1){
			return ref(_pv)(_p1);
		}
		void destroy(void *_p){
			if(sizeof(T) <= DSZ)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
};

template <unsigned DSZ, class R, class P1, class P2>
class FunctorBase<DSZ, R, P1, P2, NoType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2){
			return ref(_pv)(_p1, _p2);
		}
		void destroy(void *_p){
			if(sizeof(T) <= DSZ)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
};

template <unsigned DSZ, class R, class P1, class P2, class P3>
class FunctorBase{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2, P3){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2, P3 _p3){
			return ref(_pv)(_p1, _p2, _p3);
		}
		void destroy(void *_p){
			if(sizeof(T) <= DSZ)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
};

template <
	unsigned DSZ = sizeof(void*),
	class R = void,
	class P1 = NoType, class P2 = NoType, class P3 = NoType
>
class FunctorStub;

template <
	unsigned DSZ,
	typename R,
	typename P1, typename P2, typename P3
>
class FunctorStub: FunctorBase<
	(DSZ >= sizeof(void*)) ? DSZ : sizeof(void*),
	R, P1, P2, P3
>{
	typedef FunctorBase<(DSZ >= sizeof(void*)) ? DSZ : sizeof(void*), R, P1, P2, P3> BaseTp;
	typedef typename BaseTp::BaseCaller	BaseCallerTp;
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCallerTp;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCallerTp*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCallerTp*>(callerbuf)->~BaseCaller();
		new(callerbuf) typename BaseTp::template Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	R operator()(){
		return reinterpret_cast<BaseCallerTp*>(callerbuf)->call(storebuf);
	}
	R operator()(P1 _p1){
		return reinterpret_cast<BaseCallerTp*>(callerbuf)->call(storebuf, _p1);
	}
	R operator()(P1 _p1, P2 _p2){
		return reinterpret_cast<BaseCallerTp*>(callerbuf)->call(storebuf, _p1, _p2);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3){
		return reinterpret_cast<BaseCallerTp*>(callerbuf)->call(storebuf, _p1, _p2, _p3);
	}
private:
	char callerbuf[sizeof(BaseCallerTp)];
	char storebuf[StoreBufSize];
};

//================================================================================

struct TestAFunctor{
	TestAFunctor(int _v):v(_v){}
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
	FunctorStub<4, int>								fnca;
	FunctorStub<4, void, const std::string&>		fncb;
	FunctorStub<4, int, const std::string&>			fncc;
	FunctorStub<4, int, int, const std::string&>	fncd;
	FunctorStub<4, void, const std::string&, int>	fnce;
	FunctorStub<4, void, const std::string&, int, const char *>	fncf;
	fnca = TestAFunctor(11);
	fncb = TestBFunctor(12);
	fncc = TestCFunctor(13);
	fncd = TestDFunctor(14);
	fnce = TestEFunctor(15);
	fncf = TestFFunctor(16);
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
	return 0;
}

