#ifndef SYSTEM_DYNAMIC_HPP
#define SYSTEM_DYNAMIC_HPP

#include "system/common.hpp"
#include "system/cassert.hpp"
#include <vector>
#include "utility/dynamicpointer.hpp"

struct DynamicMap{
	typedef void (*FncTp)(const DynamicPointer<DynamicBase> &,void*);
	typedef void (*RegisterFncTp)(DynamicMap &_rdm);
	static uint32 generateId();
	
	DynamicMap(DynamicMap *_pdm);
	~DynamicMap();
	void callback(uint32 _tid, FncTp _pf);
	FncTp callback(uint32 _id)const;
	struct Data;
	Data	&d;
};

struct DynamicRegistererBase{
	void lock();
	void unlock();
};

template <class T>
struct DynamicRegisterer: DynamicRegistererBase{
	DynamicRegisterer(){
		lock();
		T::dynamicRegister();
		unlock();
	}
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
	template<class G1>
	Dynamic(G1 _g1):T(_g1){}
	template<class G1, class G2>
	Dynamic(G1 _g1, G2 _g2):T(_g1, _g2){}
	template<class G1, class G2, class G3>
	Dynamic(G1 _g1, G2 _g2, G3 _g3):T(_g1, _g2, _g3){}
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

template <class R, class Exe>
struct DynamicReceiver{
private:
	typedef R (*FncTp)(const DynamicPointer<DynamicBase> &, void*);
	
	template <class S, class E>
	static R doExecute(const DynamicPointer<DynamicBase> &_dp, void *_e){
		E *pe = reinterpret_cast<E*>(_e);
		DynamicPointer<S>	ds(_dp);
		return pe->dynamicExecute(ds);
	}
	
	typedef std::vector<DynamicPointer<DynamicBase> > DynamicPointerVectorTp;
	template <class E>
	static DynamicMap& dynamicMapEx(){
		static DynamicMap	dm(dynamicMap());
		dynamicMap(&dm);
		return dm;
	}
public:
	DynamicReceiver():pushid(0), execid(1){}
	
	void push(const DynamicPointer<DynamicBase> &_dp){
		v[pushid].push_back(_dp);
	}
	
	uint prepareExecute(){
		v[execid].clear();
		uint tmp = execid;
		execid = pushid;
		pushid = tmp;
		crtit = v[execid].begin();
		return v[execid].size();
	}
	
	inline bool hasCurrent()const{
		return crtit != v[execid].end();
	}
	void next(){
		cassert(crtit != v[execid].end());
		crtit->clear();
		++crtit;
	}
	template <typename E>
	R executeCurrent(E &_re){
		cassert(crtit != v[execid].end());
		FncTp	pf = reinterpret_cast<FncTp>((*crtit)->callback(*dynamicMap()));
		if(pf) return (*pf)(*crtit, &_re);
		return _re.dynamicExecuteDefault(*crtit);
	}
	
	static DynamicMap* dynamicMap(DynamicMap *_pdm = NULL){
		static DynamicMap *pdm(_pdm);
		if(_pdm) pdm = _pdm;
		return pdm;
	}
	
	template <class S, class E>
	static void add(){
		FncTp	pf = &doExecute<S, E>;
		dynamicMapEx<E>().callback(S::staticTypeId(), reinterpret_cast<DynamicMap::FncTp>(pf));
	}
	
private:
	DynamicPointerVectorTp						v[2];
	DynamicPointerVectorTp::iterator			crtit;
	uint										pushid;
	uint										execid;
};


#endif
