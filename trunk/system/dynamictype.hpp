#ifndef SYSTEM_DYNAMIC_HPP
#define SYSTEM_DYNAMIC_HPP

#include "system/common.hpp"

struct DynamicId{
	static uint32 generate();
	DynamicId(uint32 _v):v(_v){}
	uint32	v;
};

struct DynamicCallbackMap{
	typedef int (*FncTp)(void*,void*);
	
	static uint32 generateId();
	
	DynamicCallbackMap();
	~DynamicCallbackMap();
	void callback(uint32 _tid, FncTp _pf);
	FncTp callback(uint32 _id);
	struct Data;
	Data	&d;
};

#define DYNAMIC_DECLARATION\
	public:\
	static uint32 staticTypeId();\
	virtual uint32 dynamicTypeId()const;\
	public:

#define DYNAMIC_DEFINITION(X)\
	/*static*/ uint32 X::staticTypeId(){\
		static uint32 id(DynamicCallbackMap::generateId());\
		return id;\
	}\
	uint32 X::dynamicTypeId()const{\
		return staticTypeId();\
	}
	
#define DYNAMIC_RECEIVER_DECLARATION\
	public:\
	template <class O, class C>\
	static int dynamicExecute(void *_pcmd, void *_po);\
	private:

#define DYNAMIC_RECEIVER_DEFINITION(RCV, FNC, CMD)\
static DynamicCallbackMap& callbackMap(){\
	static DynamicCallbackMap	cm;\
	return cm;\
}\
template <class O, class C>\
/*static*/ int RCV::dynamicExecute(void *_pcmd, void *_po){\
	return static_cast<O*>(_po)->FNC(*static_cast<C*>(_pcmd));\
}\
template <class O, class C>\
static void registerCommand(){\
	callbackMap().callback(C::staticTypeId(), &O::template dynamicExecute<O, C>);\
}\
/*static*/ int executeCommand(CMD &_rcmd, void *_pthis){\
	DynamicCallbackMap::FncTp pf = callbackMap().callback(_rcmd.dynamicTypeId());\
	if(pf) return (*pf)(&_rcmd, _pthis);\
	return BAD;\
}

#endif
