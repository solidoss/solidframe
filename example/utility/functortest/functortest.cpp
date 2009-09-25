#include "system/debug.hpp"
#include "system/common.hpp"

// template <unsigned DSZ = sizeof(void*)>
// class FunctorStub{
// 	enum {
// 		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
// 	};
// 	struct BaseCaller{
// 		virtual ~BaseCaller(){}
// 		virtual void call(void *_p){}
// 		virtual void destroy(void *_p){}
// 		virtual size_t size()const{return 0;}
// 	};
// 	template <class T>
// 	struct Caller: BaseCaller{
// 		void call(void *_p){
// 			if(sizeof(T) <= StoreBufSize)
// 				(*reinterpret_cast<T*>(_p))();
// 			else
// 				(*reinterpret_cast<T*>(*reinterpret_cast<void**>(_p)))();
// 		}
// 		void destroy(void *_p){
// 			if(sizeof(T) <= StoreBufSize)
// 				reinterpret_cast<T*>(_p)->~T();
// 			else
// 				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
// 		}
// 		size_t size()const{return sizeof(T);}
// 	};
// public:
// 	FunctorStub(){
// 		new(callerbuf) BaseCaller;
// 	}
// 	template <class T>
// 	void operator=(const T &_rt){
// 		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
// 		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
// 		new(callerbuf) Caller<T>;
// 		if(sizeof(T) <= StoreBufSize){
// 			new(storebuf) T(_rt);
// 		}else{
// 			*reinterpret_cast<void**>(storebuf) = new T(_rt);
// 		}
// 	}
// 	void operator()(){
// 		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf);
// 	}
// private:
// 	char callerbuf[sizeof(BaseCaller)];
// 	char storebuf[StoreBufSize];
// }; 


// template <class R = int, unsigned DSZ = sizeof(void*)>
// class FunctorStub{
// 	enum {
// 		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
// 	};
// 	struct BaseCaller{
// 		virtual ~BaseCaller(){}
// 		virtual R call(void *_p){return R();}
// 		virtual void destroy(void *_p){}
// 		virtual size_t size()const{return 0;}
// 	};
// 	template <class T>
// 	struct Caller: BaseCaller{
// 		R call(void *_p){
// 			if(sizeof(T) <= StoreBufSize)
// 				return (*reinterpret_cast<T*>(_p))();
// 			else
// 				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_p)))();
// 		}
// 		void destroy(void *_p){
// 			if(sizeof(T) <= StoreBufSize)
// 				reinterpret_cast<T*>(_p)->~T();
// 			else
// 				reinterpret_cast<T*>(*reinterpret_cast<void**>(_p))->~T();
// 		}
// 		size_t size()const{return sizeof(T);}
// 	};
// public:
// 	FunctorStub(){
// 		new(callerbuf) BaseCaller;
// 	}
// 	template <class T>
// 	void operator=(const T &_rt){
// 		reinterpret_cast<BaseCaller*>(callerbuf)->destroy(storebuf);
// 		reinterpret_cast<BaseCaller*>(callerbuf)->~BaseCaller();
// 		new(callerbuf) Caller<T>;
// 		if(sizeof(T) <= StoreBufSize){
// 			new(storebuf) T(_rt);
// 		}else{
// 			*reinterpret_cast<void**>(storebuf) = new T(_rt);
// 		}
// 	}
// 	R operator()(){
// 		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf);
// 	}
// private:
// 	char callerbuf[sizeof(BaseCaller)];
// 	char storebuf[StoreBufSize];
// };

struct NoType{};

template <class P1, class R = int, unsigned DSZ = sizeof(void*)>
class FunctorStub{
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
		R call(void *_pv, P1 _p){
			if(sizeof(T) <= StoreBufSize)
				return (*reinterpret_cast<T*>(_pv))(_p);
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)))(_p);
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
// 	R operator()(){
// 		return reinterpret_cast<BaseCaller*>(callerbuf)->call(storebuf, NoType);
// 	}
private:
	char callerbuf[sizeof(BaseCaller)];
	char storebuf[StoreBufSize];
}; 


struct TestFunctor{
	TestFunctor(int _v):v(_v){}
	int operator()(){
		idbg("v = "<<v);
		return v;
	}
	
	int operator()(const std::string &_rs){
		idbg("v = "<<v<<" string = "<<_rs);
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
	FunctorStub<const std::string&>	fnc1;
	//fnc0 = TestFunctor(10);
	fnc1 = TestFunctor(11);
	//int v = fnc0();
	//idbg("v = "<<v);
	int v = fnc1("ceva");
	idbg("v = "<<v);
	return 0;
}
