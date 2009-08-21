#ifndef SYSTEM_DYNAMIC_HPP
#define SYSTEM_DYNAMIC_HPP

#include "system/common.hpp"
#include "system/dynamicpointer.hpp"

struct DynamicMap{
	typedef void (*FncTp)(const DynamicPointer<DynamicBase> &,void*);
	typedef void (*RegisterFncTp)(DynamicMap &_rdm);
	static uint32 generateId();
	
	DynamicMap(RegisterFncTp _prfnc);
	~DynamicMap();
	void callback(uint32 _tid, FncTp _pf);
	FncTp callback(uint32 _id)const;
	struct Data;
	Data	&d;
};

//struct DynamicPointerBase;

struct DynamicBase{
	virtual uint32 dynamicTypeId()const = 0;
	virtual DynamicMap::FncTp callback(const DynamicMap &_rdm);
	//! Used by DynamicPointer - smartpointers
	virtual void use();
	//! Used by DynamicPointer to know if the object must be deleted
	virtual int release();

protected:
	friend struct DynamicPointerBase;
	virtual ~DynamicBase();
};

template <class X, class T = DynamicBase>
struct Dynamic: T{
	typedef Dynamic<X,T>	BaseTp;
	Dynamic(){}
	template<class G>
	Dynamic(G _x):T(_x){}
	static uint32 staticTypeId(){
		static uint32 id(DynamicMap::generateId());
		return id;
	}
	//TODO: add:
	//static bool isTypeExplicit(const DynamicBase*);
	//static bool isType(const DynamicBase*);
	virtual uint32 dynamicTypeId()const{
		return staticTypeId();
	}
	virtual DynamicMap::FncTp callback(const DynamicMap &_rdm){
		DynamicMap::FncTp pf = _rdm.callback(staticTypeId());
		if(pf) return pf;
		return T::callback(_rdm);
	}
};

struct DynamicReceiverBase{
	typedef void (*FncTp)(const DynamicPointer<DynamicBase> &, void*);
};


template <class X, class T = DynamicReceiverBase>
struct DynamicReceiver: T{
	typedef DynamicReceiver<X,T>	BaseTp;
	DynamicReceiver(){}
	template<class G>
	DynamicReceiver(const G &_x):T(_x){}
// 	template<class G>
// 	DynamicReceiver(G _x):T(_x){}

	template <class C>
	static void dynamicCallback(const DynamicPointer<DynamicBase> &_pd, void *_prcv){
		DynamicPointer<C>	dp(_pd);
		return static_cast<X*>(_prcv)->dynamicReceive(dp);
	}
	static DynamicMap& dynamicMap(){
		static DynamicMap dm(&X::dynamicRegister);
		return dm;
	}
	template <class C>
	static void dynamicRegister(DynamicMap &_rdm){
		_rdm.callback(C::staticTypeId(), &dynamicCallback<C>);
	}
	void dynamicReceiver(const DynamicPointer<DynamicBase> &_pd){
		DynamicMap::FncTp pf = _pd->callback(dynamicMap());
		if(pf) (*pf)(_pd, this);
	}
};


#endif
