/* Declarations file functorstub.hpp
	
	Copyright 2010 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTILITY_FUNCTORSTUB_HPP
#define UTILITY_FUNCTORSTUB_HPP

#include "system/common.hpp"


//--------------------------------------------------------------------------------------
// 		FunctorBase
//--------------------------------------------------------------------------------------
template <
	unsigned DSZ,
	class R,
	class P1, class P2, class P3, class P4, class P5, class P6, class P7
>
class FunctorBase;

struct CallerBase{
	virtual ~CallerBase(){}
	virtual void destroy(void *_p){}
	virtual size_t size()const{return 0;}
};

template <unsigned DSZ, class R>
class FunctorBase<DSZ, R, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType>{
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
class FunctorBase<DSZ, R, P1, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType>{
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
class FunctorBase<DSZ, R, P1, P2, EmptyType, EmptyType, EmptyType, EmptyType, EmptyType>{
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
class FunctorBase<DSZ, R, P1, P2, P3, EmptyType, EmptyType, EmptyType, EmptyType>{
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

template <unsigned DSZ, class R, class P1, class P2, class P3, class P4>
class FunctorBase<DSZ, R, P1, P2, P3, P4, EmptyType, EmptyType, EmptyType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2, P3, P4){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2, P3 _p3, P4 _p4){
			return ref(_pv)(_p1, _p2, _p3, _p4);
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
template <unsigned DSZ, class R, class P1, class P2, class P3, class P4, class P5>
class FunctorBase<DSZ, R, P1, P2, P3, P4, P5, EmptyType, EmptyType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2, P3, P4, P5){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5){
			return ref(_pv)(_p1, _p2, _p3, _p4, _p5);
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
template <unsigned DSZ, class R, class P1, class P2, class P3, class P4, class P5, class P6>
class FunctorBase<DSZ, R, P1, P2, P3, P4, P5, P6, EmptyType>{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2, P3, P4, P5, P6){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5, P6 _p6){
			return ref(_pv)(_p1, _p2, _p3, _p4, _p5, _p6);
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

template <unsigned DSZ, class R, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
class FunctorBase{
protected:
	struct BaseCaller: CallerBase{
		virtual R call(void *_p, P1, P2, P3, P4, P5, P6, P7){return R();}
	};
	template <class T>
	struct Caller: BaseCaller{
		inline T& ref(void* _pv){
			if(sizeof(T) <= DSZ)
				return (*reinterpret_cast<T*>(_pv));
			else
				return (*reinterpret_cast<T*>(*reinterpret_cast<void**>(_pv)));
		}
		R call(void *_pv, P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5, P6 _p6, P7 _p7){
			return ref(_pv)(_p1, _p2, _p3, _p4, _p5, _p6, _p7);
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

//--------------------------------------------------------------------------------------
//		FunctorStub
//--------------------------------------------------------------------------------------

template <
	unsigned DSZ = sizeof(void*),
	class R = void,
	class P1 = EmptyType, class P2 = EmptyType, class P3 = EmptyType, class P4 = EmptyType, class P5 = EmptyType, class P6 = EmptyType, class P7 = EmptyType
>
class FunctorStub;

template <
	unsigned DSZ,
	typename R,
	typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7
>
class FunctorStub: FunctorBase<
	(DSZ >= sizeof(void*)) ? DSZ : sizeof(void*),
	R, P1, P2, P3, P4, P5, P6, P7
>{
	typedef FunctorBase<(DSZ >= sizeof(void*)) ? DSZ : sizeof(void*), R, P1, P2, P3, P4, P5, P6, P7> BaseT;
	typedef typename BaseT::BaseCaller	BaseCallerT;
	enum {
		StoreBufSize = (DSZ >= sizeof(void*)) ? DSZ : sizeof(void*)
	};
public:
	FunctorStub(){
		new(callerbuf) BaseCallerT;
	}
	~FunctorStub(){
		reinterpret_cast<BaseCallerT*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCallerT*>(callerbuf)->~BaseCallerT();
	}
	void clear(){
		reinterpret_cast<BaseCallerT*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCallerT*>(callerbuf)->~BaseCallerT();
		new(callerbuf) BaseCallerT;
	}
	template <class T>
	void operator=(const T &_rt){
		reinterpret_cast<BaseCallerT*>(callerbuf)->destroy(storebuf);
		reinterpret_cast<BaseCallerT*>(callerbuf)->~BaseCallerT();
		new(callerbuf) typename BaseT::template Caller<T>;
		if(sizeof(T) <= StoreBufSize){
			new(storebuf) T(_rt);
		}else{
			*reinterpret_cast<void**>(storebuf) = new T(_rt);
		}
	}
	R operator()(){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf);
	}
	R operator()(P1 _p1){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1);
	}
	R operator()(P1 _p1, P2 _p2){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2, _p3);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2, _p3, _p4);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2, _p3, _p4, _p5);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5, P6 _p6){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2, _p3, _p4, _p5, _p6);
	}
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4, P5 _p5, P6 _p6, P7 _p7){
		return reinterpret_cast<BaseCallerT*>(callerbuf)->call(storebuf, _p1, _p2, _p3, _p4, _p5, _p6, _p7);
	}
private:
	char callerbuf[sizeof(BaseCallerT)];
	char storebuf[StoreBufSize];
};


#endif
