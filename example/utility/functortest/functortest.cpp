#include "system/debug.hpp"
#include "system/common.hpp"

struct NoType{};

template <
	unsigned DSZ = sizeof(void*),
	class R = int,
	class P1 = NoType, class P2 = NoType, class P3 = NoType
>
class FunctorStub;

template <unsigned DSZ, class R>
class FunctorStub<DSZ, R, NoType, NoType, NoType>{
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
	struct BaseCaller{
		virtual ~BaseCaller(){}
		virtual R call(void *_p){return R();}
		virtual void destroy(void *_p){}
		virtual size_t size()const{return 0;}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv){
			return ref(_pv)();
		}
		void destroy(void *_p){
			if(sizeof(T) <= StoreBufSize)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCaller;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
		new(callerbuf) Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	R operator()(){
		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf);
	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
};

template <unsigned DSZ, class P1>
class FunctorStub<DSZ, void, P1, NoType, NoType>{
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
	struct BaseCaller{
		virtual ~BaseCaller(){}
		virtual void call(void *_p, P1){}
		virtual void destroy(void *_p){}
		virtual size_t size()const{return 0;}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		void call(void *_pv, P1 _p1){
			ref(_pv)(_p1);
		}
		void destroy(void *_p){
			if(sizeof(T) <= StoreBufSize)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCaller;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
		new(callerbuf) Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	void operator()(P1 _p){
		reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf, _p);
	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
};


template <unsigned DSZ, class R, class P1>
class FunctorStub<DSZ, R, P1, NoType, NoType>{
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
	struct BaseCaller{
		virtual ~BaseCaller(){}
		virtual R call(void *_p, P1){return R();}
		virtual void destroy(void *_p){}
		virtual size_t size()const{return 0;}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1){
			return ref(_pv)(_p1);
		}
		void destroy(void *_p){
			if(sizeof(T) <= StoreBufSize)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCaller;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
		new(callerbuf) Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	R operator()(P1 _p){
		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf, _p);
	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
};

template <unsigned DSZ, class P1, class P2>
class FunctorStub<DSZ, void, P1, P2, NoType>{
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
	struct BaseCaller{
		virtual ~BaseCaller(){}
		virtual void call(void *_p, P1, P2){}
		virtual void destroy(void *_p){}
		virtual size_t size()const{return 0;}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		void call(void *_pv, P1 _p1, P2 _p2){
			ref(_pv)(_p1, _p2);
		}
		void destroy(void *_p){
			if(sizeof(T) <= StoreBufSize)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCaller;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
		new(callerbuf) Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	void operator()(P1 _p1, P2 _p2){
		reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf, _p1, _p2);
	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
};

template <unsigned DSZ, class R, class P1, class P2>
class FunctorStub<DSZ, R, P1, P2, NoType>{
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
	struct BaseCaller{
		virtual ~BaseCaller(){}
		virtual R call(void *_p, P1, P2){return R();}
		virtual void destroy(void *_p){}
		virtual size_t size()const{return 0;}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2){
			return ref(_pv)(_p1, _p2);
		}
		void destroy(void *_p){
			if(sizeof(T) <= StoreBufSize)
				reinterpret_cast<T*>(_p)->~T();
			else
				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
		}
		size_t size()const{return sizeof(T);}
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCaller;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
		new(callerbuf) Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	R operator()(P1 _p1, P2 _p2){
		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf, _p1, _p2);
	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
};

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
	fnca = TestAFunctor(11);
	fncb = TestBFunctor(12);
	fncc = TestCFunctor(13);
	fncd = TestDFunctor(14);
	fnce = TestEFunctor(15);
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
	return 0;
}

