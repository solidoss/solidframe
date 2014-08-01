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
	virtual void onEvent(Context &_rctx);
	virtual void onEvent(Context &_rctx, size_t _data);
	virtual void onEvent(Context &_rctx, std::string const &_data);
private:
	ATOMIC_NS::atomic<size_t>	id;
};

/*virtual*/ void Object::onEvent(Context &_rctx){
	
}
/*virtual*/ void Object::onEvent(Context &_rctx, size_t _data){
	
}
/*virtual*/ void Object::onEvent(Context &_rctx, std::string const &_data){
	
}

struct Context{
	size_t 	a;
	size_t	b;
	Context(size_t _a, size_t _b):a(_a), b(_b){}
};

struct ObjectStub{
	DynamicPointer<Object>	objptr;
	size_t					a;
	size_t					b;
	
	void registerObject(size_t _idx){
		objptr->id = _idx;
	}
	
	void call(std::string const& _data){
		Context ctx(a, b);
		objptr->onEvent(ctx);
		objptr->onEvent(ctx, _data.size());
		objptr->onEvent(ctx, _data);
	}
};

class FirstObject: public Dynamic<FirstObject, Object>{
private:
	/*virtual*/ void onEvent(Context &_rctx);
	/*virtual*/ void onEvent(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void FirstObject::onEvent(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void FirstObject::onEvent(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void FirstObject::onEvent(Context &_rctx, std::string const &_data){
	str = _data;
}

class SecondObject: public Dynamic<SecondObject, Object>{
private:
	/*virtual*/ void onEvent(Context &_rctx);
	/*virtual*/ void onEvent(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void SecondObject::onEvent(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void SecondObject::onEvent(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void SecondObject::onEvent(Context &_rctx, std::string const &_data){
	str = _data;
}

class ThirdObject: public Dynamic<ThirdObject, Object>{
private:
	/*virtual*/ void onEvent(Context &_rctx);
	/*virtual*/ void onEvent(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void ThirdObject::onEvent(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void ThirdObject::onEvent(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void ThirdObject::onEvent(Context &_rctx, std::string const &_data){
	str = _data;
}

class FourthObject: public Dynamic<FourthObject, Object>{
private:
	/*virtual*/ void onEvent(Context &_rctx);
	/*virtual*/ void onEvent(Context &_rctx, size_t _data);
	/*virtual*/ void onEvent(Context &_rctx, std::string const &_data);
private:
	std::string str;
	size_t		val;
};

/*virtual*/ void FourthObject::onEvent(Context &_rctx){
	val ^= _rctx.a;
	val ^= _rctx.b;
}
/*virtual*/ void FourthObject::onEvent(Context &_rctx, size_t _data){
	val += _data;
}
/*virtual*/ void FourthObject::onEvent(Context &_rctx, std::string const &_data){
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
		switch(i % 4){
			case 0: stub.objptr = new FirstObject;break;
			case 1: stub.objptr = new SecondObject;break;
			case 2: stub.objptr = new ThirdObject;break;
			case 3: stub.objptr = new FourthObject;break;
		}
		stub.a = objdq.size();
		stub.b = objdq.size()/2;
		objdq.push_back(stub);
		objdq.back().registerObject(objdq.size() - 1);
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
