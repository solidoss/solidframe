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

struct Context{
	uint32 idx;
};

class FirstHandler{
protected:
	typedef DynamicMapper<int, FirstHandler, Context>	DynamicMapperT;
public:
	FirstHandler(){
	}
	static void init(){
		dm.insert<AObject, FirstHandler>();
		dm.insert<BObject, FirstHandler>();
	}
	void push(const DynamicPointer<> &_dp){
		dv.push_back(_dp);
	}
	void clear(){
		dv.clear();
	}
	int dynamicHandle(DynamicPointer<> &_dp, Context &_rctx);
	int dynamicHandle(const DynamicPointer<AObject> &_rdp, Context &_rctx);
	int dynamicHandle(const DynamicPointer<BObject> &_rdp, Context &_rctx);
	
protected:
	typedef std::vector<DynamicPointer<> >	DynamicPointerVectorT;
	static DynamicMapperT	dm;
	DynamicPointerVectorT	dv;
};

/*static*/ FirstHandler::DynamicMapperT FirstHandler::dm;

int FirstHandler::dynamicHandle(DynamicPointer<> &_dp, Context &_rctx){
	idbg("");
	return -1;
}

int FirstHandler::dynamicHandle(const DynamicPointer<AObject> &_dp, Context &_rctx){
	idbg("v = "<<_dp->v);
	return _dp->v;
}

int FirstHandler::dynamicHandle(const DynamicPointer<BObject> &_dp, Context &_rctx){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
	return _dp->v2;
}

class SecondHandler: public FirstHandler{
public:
	SecondHandler(){
	}
	
	static void init(){
		FirstHandler::init();
		dm.insert<BObject, SecondHandler>();
		dm.insert<CObject, SecondHandler>();
		dm.insert<DObject, SecondHandler>();
	}
	
	void run();
	
	int dynamicHandle(const DynamicPointer<BObject> &_dp, Context &_rctx);
	int dynamicHandle(const DynamicPointer<CObject> &_dp, Context &_rctx);
	int dynamicHandle(const DynamicPointer<DObject> &_dp, Context &_rctx);
};


void SecondHandler::run(){
	DynamicHandler<DynamicMapperT, 6>	dh(dm, dv.begin(), dv.end());
	dv.clear();
	Context ctx;
	for(size_t i = 0; i < dh.size(); ++i){
		dh.handle(*this, i, ctx);
	}
}

int SecondHandler::dynamicHandle(const DynamicPointer<BObject> &_dp, Context &_rctx){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
	return _dp->v1;
}
int SecondHandler::dynamicHandle(const DynamicPointer<CObject> &_dp, Context &_rctx){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2<<" v3 "<<_dp->v3);
	return _dp->v1;
}
int SecondHandler::dynamicHandle(const DynamicPointer<DObject> &_dp, Context &_rctx){
	idbg("s = "<<_dp->s<<" v = "<<_dp->v);
	return _dp->v;
}


class ThirdHandler{
protected:
	typedef DynamicMapper<void, ThirdHandler, Context>	DynamicMapperT;
public:
	ThirdHandler(){
	}
	static void init(){
		dm.insert<AObject, ThirdHandler>();
		dm.insert<BObject, ThirdHandler>();
	}
	void push(const DynamicPointer<> &_dp){
		dv.push_back(_dp);
	}
	void clear(){
		dv.clear();
	}
	void run();
	void dynamicHandle(DynamicPointer<> &_dp, Context &_rctx);
	void dynamicHandle(const DynamicPointer<AObject> &_rdp, Context &_rctx);
	void dynamicHandle(const DynamicPointer<BObject> &_rdp, Context &_rctx);
	
protected:
	typedef std::vector<DynamicPointer<> >	DynamicPointerVectorT;
	static DynamicMapperT	dm;
	DynamicPointerVectorT	dv;
};

/*static*/ ThirdHandler::DynamicMapperT ThirdHandler::dm;

void ThirdHandler::dynamicHandle(DynamicPointer<> &_dp, Context &_rctx){
	idbg("");
}

void ThirdHandler::dynamicHandle(const DynamicPointer<AObject> &_dp, Context &_rctx){
	idbg("v = "<<_dp->v);
}

void ThirdHandler::dynamicHandle(const DynamicPointer<BObject> &_dp, Context &_rctx){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
}

void ThirdHandler::run(){
	DynamicHandler<DynamicMapperT, 6>	dh(dm, dv.begin(), dv.end());
	dv.clear();
	Context ctx;
	for(size_t i = 0; i < dh.size(); ++i){
		dh.handle(*this, i, ctx);
	}
}


class FourthHandler{
protected:
	typedef DynamicMapper<void, FourthHandler>	DynamicMapperT;
public:
	FourthHandler(){
	}
	static void init(){
		dm.insert<AObject, FourthHandler>();
		dm.insert<BObject, FourthHandler>();
	}
	void push(const DynamicPointer<> &_dp){
		dv.push_back(_dp);
	}
	void clear(){
		dv.clear();
	}
	void run();
	void dynamicHandle(DynamicPointer<> &_dp);
	void dynamicHandle(const DynamicPointer<AObject> &_rdp);
	void dynamicHandle(const DynamicPointer<BObject> &_rdp);
	
protected:
	typedef std::vector<DynamicPointer<> >	DynamicPointerVectorT;
	static DynamicMapperT	dm;
	DynamicPointerVectorT	dv;
};

/*static*/ FourthHandler::DynamicMapperT FourthHandler::dm;

void FourthHandler::dynamicHandle(DynamicPointer<> &_dp){
	idbg("");
}

void FourthHandler::dynamicHandle(const DynamicPointer<AObject> &_dp){
	idbg("v = "<<_dp->v);
}

void FourthHandler::dynamicHandle(const DynamicPointer<BObject> &_dp){
	idbg("v1 = "<<_dp->v1<<" v2 = "<<_dp->v2);
}

void FourthHandler::run(){
	DynamicHandler<DynamicMapperT, 6>	dh(dm, dv.begin(), dv.end());
	dv.clear();
	for(size_t i = 0; i < dh.size(); ++i){
		dh.handle(*this, i);
	}
}

int main(){
#ifdef UDEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	Debug::the().initStdErr(false);
#endif
	
	{
		DynamicSharedPointer<AObject> 	dsap(new AObject(1));
		DynamicSharedPointer<>			dsbp;
		dsbp = dsap;
		{
			DynamicPointer<AObject>			dap(dsap);
			DynamicSharedPointer<>			dsaap(dap);
			idbg("ptr = "<<(void*)dap.get()<<" ptr = "<<(void*)dsaap.get());
		}
		idbg("ptr = "<<(void*)dsap.get()<<" ptr = "<<(void*)dsbp.get());
	}
	
	SecondHandler::init();
	
	SecondHandler	h;
	
	h.push(DynamicPointer<>(new AObject(1)));
	h.push(DynamicPointer<>(new BObject(2,3)));
	h.push(DynamicPointer<>(new CObject(1,2,3)));
	h.push(DynamicPointer<>(new DObject("hello1", 4)));
	
	h.run();
	
	h.push(DynamicPointer<>(new AObject(11)));
	h.push(DynamicPointer<>(new BObject(22,33)));
	h.push(DynamicPointer<>(new CObject(11,22,33)));
	h.push(DynamicPointer<>(new DObject("hello2", 44)));
	
	h.run();
	
	h.push(DynamicPointer<>(new AObject(111)));
	h.push(DynamicPointer<>(new BObject(222,333)));
	h.push(DynamicPointer<>(new CObject(111,222,333)));
	h.push(DynamicPointer<>(new DObject("hello3", 444)));
	
	h.run();
	
	ThirdHandler::init();
	ThirdHandler	h3;
	
	h3.push(DynamicPointer<>(new AObject(1)));
	h3.push(DynamicPointer<>(new BObject(2,3)));
	h3.push(DynamicPointer<>(new CObject(1,2,3)));
	h3.push(DynamicPointer<>(new DObject("hello1", 4)));
	
	h3.run();
	
	FourthHandler::init();
	FourthHandler	h4;
	
	h4.push(DynamicPointer<>(new AObject(1)));
	h4.push(DynamicPointer<>(new BObject(2,3)));
	h4.push(DynamicPointer<>(new CObject(1,2,3)));
	h4.push(DynamicPointer<>(new DObject("hello1", 4)));
	
	h4.run();
	return 0;
}
