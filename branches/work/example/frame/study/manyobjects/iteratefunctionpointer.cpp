#include <iostream>
#include <deque>
#include <string>
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

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

typedef void(*Cbk1T)(Object*, Context&);
typedef void(*Cbk2T)(Object*, Context&, size_t);
typedef void(*Cbk3T)(Object*, Context&, std::string const&);

struct ObjectStub{
	DynamicPointer<Object>	objptr;
	Object					*pobj;
	size_t					a;
	size_t					b;
	
	Cbk1T					cbk1;
	Cbk2T					cbk2;
	Cbk3T					cbk3;
	
	template <class Obj>
	static void call1(Object* _pobj, Context& _rctx){
		static_cast<Obj*>(_pobj)->onEvent1(_rctx);
	}
	template <class Obj>
	static void call2(Object* _pobj, Context& _rctx, size_t _v){
		static_cast<Obj*>(_pobj)->onEvent2(_rctx, _v);
	}
	template <class Obj>
	static void call3(Object* _pobj, Context& _rctx, std::string const& _v){
		static_cast<Obj*>(_pobj)->onEvent3(_rctx, _v);
	}
	
	template<class Obj>
	void registerObject(size_t _idx){
		objptr->id = _idx;
		pobj  = objptr.get();
		cbk1 = &ObjectStub::call1<Obj>;
		cbk2 = &ObjectStub::call2<Obj>;
		cbk3 = &ObjectStub::call3<Obj>;
	}
	
	void call(std::string const& _data)const{
		Context ctx(a, b);
		(cbk1)(pobj, ctx);
		(cbk2)(pobj, ctx, _data.size());
		(cbk3)(pobj, ctx, _data);
	}
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


int main(int argc, char *argv[]){
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
