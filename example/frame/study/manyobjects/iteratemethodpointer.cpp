#include <iostream>
#include <deque>
#include <string>
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/function.hpp"
#include "system/atomic.hpp"
#include <mcheck.h>
#include <malloc.h>

using namespace std;
using namespace solid;

struct ObjectStub;
struct Context;

class Object: public Dynamic<Object, DynamicShared<> >{
protected:
	Object(){}
	virtual ~Object(){}
private:
	friend struct ObjectStub;
private:
	ATOMIC_NS::atomic<size_t>	id;
};


struct Context{
	size_t 	a;
	size_t	b;
	Context(size_t _a, size_t _b):a(_a), b(_b){}
};

class FirstObject: public Dynamic<FirstObject, Object>{
public:
	/*virtual*/ void onEvent1(Context &_rctx);
	/*virtual*/ void onEvent2(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent3(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void FirstObject::onEvent1(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void FirstObject::onEvent2(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void FirstObject::onEvent3(Context &_rctx, std::string const &_data){
	str = _data;
}

class SecondObject: public Dynamic<SecondObject, Object>{
public:
	/*virtual*/ void onEvent1(Context &_rctx);
	/*virtual*/ void onEvent2(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent3(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void SecondObject::onEvent1(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void SecondObject::onEvent2(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void SecondObject::onEvent3(Context &_rctx, std::string const &_data){
	str = _data;
}

class ThirdObject: public Dynamic<ThirdObject, Object>{
public:
	/*virtual*/ void onEvent1(Context &_rctx);
	/*virtual*/ void onEvent2(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent3(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void ThirdObject::onEvent1(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void ThirdObject::onEvent2(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void ThirdObject::onEvent3(Context &_rctx, std::string const &_data){
	str = _data;
}

class FourthObject: public Dynamic<FourthObject, Object>{
public:
	/*virtual*/ void onEvent1(Context &_rctx);
	/*virtual*/ void onEvent2(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent3(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void FourthObject::onEvent1(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void FourthObject::onEvent2(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void FourthObject::onEvent3(Context &_rctx, std::string const &_data){
	str = _data;
}

template <class R = void, class P1 = void, class P2 = void, class P3 = void>
class Delegate;

template <class R, class P1>
class Delegate<R, P1, void, void>{
public:
	typedef Delegate<R, P1, void, void>	ThisT;
	Delegate():pobj(NULL), cbk(NULL){}
	
	template <class O, R(O::*TF)(P1)>
	static ThisT create(O *_po){
		ThisT t;
		t.pobj = _po;
		t.cbk = &dlg_fnc<O, TF>;
		return t;
	}
	
	R operator()(P1 _p1)const{
		return (*cbk)(pobj, _p1);
	}
private:
	template <class O, R(O::*TF)(P1)>
	static R dlg_fnc(void *_po, P1 _p1){
		return (static_cast<O*>(_po)->*TF)(_p1);
	}
private:
	typedef R (*CbkT)(void*, P1);
	
	void	*pobj;
	CbkT	cbk;
};

template <class R, class P1, class P2>
class Delegate<R, P1, P2, void>{
public:
	typedef Delegate<R, P1, P2, void>	ThisT;
	Delegate():pobj(NULL), cbk(NULL){}
	
	template <class O, R(O::*TF)(P1, P2)>
	static ThisT create(O *_po){
		ThisT t;
		t.pobj = _po;
		t.cbk = &dlg_fnc<O, TF>;
		return t;
	}
	
	R operator()(P1 _p1, P2 _p2)const{
		return (*cbk)(pobj, _p1, _p2);
	}
private:
	template <class O, R(O::*TF)(P1, P2)>
	static R dlg_fnc(void *_po, P1 _p1, P2 _p2){
		return (static_cast<O*>(_po)->*TF)(_p1, _p2);
	}
private:
	typedef R (*CbkT)(void*, P1, P2);
	
	void	*pobj;
	CbkT	cbk;
};

#define USE_DELEGATE 1
//#define TEST_BIND

#ifdef TEST_BIND
#undef USE_DELEGATE
#endif

#ifdef USE_DELEGATE
typedef Delegate<void, Context&>						Fnc1T;
typedef Delegate<void, Context&, size_t>				Fnc2T;
typedef Delegate<void, Context&, std::string const&>	Fnc3T;
#else

typedef FUNCTION_NS::function<void(Context&)>						Fnc1T;
typedef FUNCTION_NS::function<void(Context&, size_t)>				Fnc2T;
typedef FUNCTION_NS::function<void(Context&, std::string const&)>	Fnc3T;

#endif

struct ObjectStub{
	DynamicPointer<Object>	objptr;
	size_t					a;
	size_t					b;
	
	Fnc1T					cbk1;
	Fnc2T					cbk2;
	Fnc3T					cbk3;
	
	
	template<class Obj>
	void registerObject(size_t _idx){
		objptr->id = _idx;
#ifdef USE_DELEGATE
		cbk1 = Fnc1T::create<Obj, &Obj::onEvent1>(static_cast<Obj*>(objptr.get()));
		cbk2 = Fnc2T::create<Obj, &Obj::onEvent2>(static_cast<Obj*>(objptr.get()));
		cbk3 = Fnc3T::create<Obj, &Obj::onEvent3>(static_cast<Obj*>(objptr.get()));
#else
		cbk1 = std::bind(&Obj::onEvent1, static_cast<Obj*>(objptr.get()), std::placeholders::_1);
		cbk2 = std::bind(&Obj::onEvent2, static_cast<Obj*>(objptr.get()), std::placeholders::_1, std::placeholders::_2);
		cbk3 = std::bind(&Obj::onEvent3, static_cast<Obj*>(objptr.get()), std::placeholders::_1, std::placeholders::_2);
#endif
	}
	
	void call(std::string const& _data){
		Context ctx(a, b);
		(cbk1)(ctx);
		(cbk2)(ctx, _data.size());
		(cbk3)(ctx, _data);
	}
};
#ifdef TEST_BIND
static void *(*old_malloc_hook)(size_t, const void *);

static void *new_malloc_hook(size_t size, const void *caller) {
    void *mem;

    __malloc_hook = old_malloc_hook;
    mem = malloc(size);
    fprintf(stderr, "%p: malloc(%zu) = %p\n", caller, size, mem);
    __malloc_hook = new_malloc_hook;

    return mem;
}

static void init_my_hooks(void) {
    old_malloc_hook = __malloc_hook;
    __malloc_hook = new_malloc_hook;
}

void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) = init_my_hooks;

void* operator new(size_t sz) throw (std::bad_alloc)
{
    cerr << "allocating " << sz << " bytes\n";
    void* mem = malloc(sz);
    if (mem)
        return mem;
    else
        throw std::bad_alloc();
}


void operator delete(void* ptr) throw()
{
    cerr << "deallocating at " << ptr << endl;
    free(ptr);
}
#endif

int main(int argc, char *argv[]){
#ifdef TEST_BIND
	FirstObject 	obj;
	SecondObject 	obj1;
	cout<<"Begin"<<endl;
	Fnc1T			fnc = std::bind(&FirstObject::onEvent1, &obj, std::placeholders::_1);
	fnc = std::bind(&SecondObject::onEvent1, &obj1, std::placeholders::_1);
	cout<<"End"<<endl;
	return 0;
#endif
	size_t objectcount = 1000000;
	size_t loopcount = 10;
	if(argc >= 2){
		objectcount = atoi(argv[1]);
	}
	if(argc == 3){
		loopcount = atoi(argv[2]);
	}
	
	deque<ObjectStub>	objdq;
	
	for(size_t i = 0; i < objectcount; ++i){
		ObjectStub	stub;
		stub.a = objdq.size();
		stub.b = objdq.size()/2;
		
		switch(i % 4){
			case 0: stub.objptr = new FirstObject; stub.registerObject<FirstObject>(objdq.size()); break;
			case 1: stub.objptr = new SecondObject; stub.registerObject<SecondObject>(objdq.size()); break;
			case 2: stub.objptr = new ThirdObject; stub.registerObject<ThirdObject>(objdq.size()); break;
			case 3: stub.objptr = new FourthObject; stub.registerObject<FourthObject>(objdq.size()); break;
		}
		
		objdq.push_back(stub);
	}
	
	std::string data("some");
	for(size_t i = 0; i < 10; ++i){
		data += data;
	}
	
	for(size_t i = 0; i < loopcount; ++i){
		for(deque<ObjectStub>::iterator it(objdq.begin()); it != objdq.end(); ++it){
			it->call(data);
		}
		data.resize(data.size() / 2);
	}
	
	return 0;
}
