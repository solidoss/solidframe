#include <iostream>
#include "system/debug.hpp"
#include "utility/dynamictype.hpp"

using namespace std;
using namespace solid;

struct AObject: Dynamic<AObject, DynamicShared<> >{
	AObject(int _v):v(_v){}
	int v;
};

struct BObject: Dynamic<BObject>{
	BObject(int _v1, int _v2):v1(_v1), v2(_v2){}
	int v1;
	int v2;
};

struct CObject: Dynamic<CObject, BObject>{
	CObject(int _v1, int _v2, int _v3):Dynamic<CObject, BObject>(_v1, _v2), v3(_v3){}
	int v3;
};

struct DObject: Dynamic<DObject, AObject>{
	DObject(const string& _s, int _v): Dynamic<DObject, AObject>(_v), s(_s){}
	string s;
};


class FirstHandler{
protected:
	typedef DynamicHandler<int, FirstHandler>	IntDynamicHandlerT;
public:
	FirstHandler(){
	}
	static void dynamicRegister(){
		IntDynamicHandlerT::registerDynamic<AObject, FirstHandler>();
		IntDynamicHandlerT::registerDynamic<BObject, FirstHandler>();
	}
	void push(const DynamicPointer<> &_dp){
		de.push(*this, _dp);
	}
	int dynamicHandle(DynamicPointer<> &_dp);
	int dynamicHandle(const DynamicPointer<AObject> &_rdp);
	int dynamicHandle(const DynamicPointer<BObject> &_rdp);
	
protected:
	IntDynamicHandlerT		de;
};

int FirstHandler::dynamicHandle(DynamicPointer<> &_dp){
	idbg("");
	return -1;
}

int FirstHandler::dynamicHandle(const DynamicPointer<AObject> &_dp){
	idbg("v = "<<_dp->v);
	return _dp->v;
}

int FirstHandler::dynamicHandle(const DynamicPointer<BObject> &_dp){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
	return _dp->v2;
}

class SecondHandler: public FirstHandler{
public:
	SecondHandler(){
	}
	
	static void dynamicRegister(){
		FirstHandler::dynamicRegister();
		IntDynamicHandlerT::registerDynamic<BObject, SecondHandler>();
		IntDynamicHandlerT::registerDynamic<CObject, SecondHandler>();
		IntDynamicHandlerT::registerDynamic<DObject, SecondHandler>();
	}
	
	void run();
	
	int dynamicHandle(const DynamicPointer<BObject> &_dp);
	int dynamicHandle(const DynamicPointer<CObject> &_dp);
	int dynamicHandle(const DynamicPointer<DObject> &_dp);
};


void SecondHandler::run(){
	int rv = de.prepareHandle(*this);
	idbg("Executing "<<rv<<" calls");
	while(de.hasCurrent(*this)){
		rv = de.handleCurrent(*this);
		idbg("call returned "<<rv);
		de.next(*this);
	}
	//dr.executeCurrent(*this);
}

int SecondHandler::dynamicHandle(const DynamicPointer<BObject> &_dp){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
	return _dp->v1;
}
int SecondHandler::dynamicHandle(const DynamicPointer<CObject> &_dp){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2<<" v3 "<<_dp->v3);
	return _dp->v1;
}
int SecondHandler::dynamicHandle(const DynamicPointer<DObject> &_dp){
	idbg("s = "<<_dp->s<<" v = "<<_dp->v);
	return _dp->v;
}

static const DynamicRegisterer<SecondHandler>	dre;

int main(){
#ifdef UDEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr(false);
#endif
	{
		DynamicSharedPointer<AObject> 	dsap(new AObject(1));
		DynamicSharedPointer<>			dsbp;
		dsbp = dsap;
		{
			DynamicPointer<AObject>			dap(dsap);
			DynamicSharedPointer<>			dsaap(dap);
			cout<<"ptr = "<<(void*)dap.get()<<" ptr = "<<(void*)dsaap.get()<<endl;
		}
		cout<<"ptr = "<<(void*)dsap.get()<<" ptr = "<<(void*)dsbp.get()<<endl;
	}
	
	SecondHandler	e;
	e.push(DynamicPointer<>(new AObject(1)));
	e.push(DynamicPointer<>(new BObject(2,3)));
	e.push(DynamicPointer<>(new CObject(1,2,3)));
	e.push(DynamicPointer<>(new DObject("hello1", 4)));
	e.run();
	e.push(DynamicPointer<>(new AObject(11)));
	e.push(DynamicPointer<>(new BObject(22,33)));
	e.push(DynamicPointer<>(new CObject(11,22,33)));
	e.push(DynamicPointer<>(new DObject("hello2", 44)));
	e.run();
	e.push(DynamicPointer<>(new AObject(111)));
	e.push(DynamicPointer<>(new BObject(222,333)));
	e.push(DynamicPointer<>(new CObject(111,222,333)));
	e.push(DynamicPointer<>(new DObject("hello3", 444)));
	e.run();
	return 0;
}
