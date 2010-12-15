#include <iostream>
#include <vector>
#include "system/common.hpp"
#include "utility/dynamictype.hpp"
#include "system/mutex.hpp"

using namespace std;

typedef uint32 IndexT;

#ifndef USERVICEBITS
//by default we have at most 32 services for x86 bits machines and 256 for x64
#define USERVICEBITS (sizeof(IndexT) == 4 ? 5 : 8)
#endif

#define ID_MASK 0xffffffff

enum ObjectDefs{
	SERVICEBITCNT = USERVICEBITS,
	INDEXBITCNT	= sizeof(IndexT) * 8 - SERVICEBITCNT,
};

typedef std::pair<IndexT, uint32>	UidT;

typedef UidT						ObjectUidT;

inline UidT invalid_uid(){
	return UidT(0xffffffff, 0xffffffff);
}

inline bool is_valid_uid(const UidT &_ruid){
	return _ruid.first != 0xffffffff;
}

inline bool is_invalid_uid(const UidT &_ruid){
	return _ruid.first == 0xffffffff;
}

inline UidT make_object_uid(const IndexT _srvidx, const IndexT _objidx, const uint32 _uid){
	return UidT(_objidx /*TODO: add _srvidx*/, _uid);
}

inline IndexT compute_index(IndexT _fullid){
	return _fullid & (ID_MASK >> SERVICEBITCNT);
}
inline IndexT compute_service_id(IndexT _fullid){
	return _fullid >> INDEXBITCNT;
}
inline IndexT compute_id(IndexT _srvid, IndexT _idx){
	return (_srvid << INDEXBITCNT) | _idx;
}


template <class V>
typename V::value_type& safe_at(V &_v, uint _pos){
	if(_pos < _v.size()){
		return _v[_pos];
	}else{
		_v.resize(_pos + 1);
		return _v[_pos];
	}
}

class Service;
class Signal;

struct Object: Dynamic<Object>{
public:
	Object(){}
	virtual ~Object();
private:
	friend class Service;
	void typeId(const uint16 _tid){
		tid = _tid;
	}
	const uint typeId()const{
		return tid;
	}
private:
	IndexT	idx;
	uint16	tid;
};

Object::~Object(){
	
}

struct Visitor: Dynamic<Visitor>{
	virtual void visitObject(Object &_robj) = 0;
};

class Service: public Dynamic<Service, Object>{
	typedef void (*EraseCbkT) (const Object &, Service *);
	typedef void (*InsertCbkT) (Object *, Service *, const ObjectUidT &);
	typedef void (*VisitCbkT)(Object *, Visitor&);
	
	template <class O, class S>
	static void insert_cbk(Object *_po, Service *_ps, const ObjectUidT &_ruid){
		static_cast<S*>(_ps)->insertObject(*static_cast<O*>(_po), _ruid);
	}
	template <class O, class S>
	static void erase_cbk(const Object &_ro, Service *_ps){
		static_cast<S*>(_ps)->eraseObject(static_cast<const O&>(_ro));
	}
	template <class O, class V>
	static void visit_cbk(Object *_po, Visitor &_rv){
		static_cast<V&>(_rv).visitObject(static_cast<O&>(*_po));
	}
	
	struct ObjectTypeStub{
		ObjectTypeStub(
			EraseCbkT _pec = &erase_cbk<Object,Service>,
			InsertCbkT _pic = &insert_cbk<Object,Service>
		):erase_callback(_pec), insert_callback(_pic){}
		bool empty()const{
			return erase_callback == NULL;
		}
		EraseCbkT	erase_callback;
		InsertCbkT	insert_callback;
	};
	
	struct VisitorTypeStub{
		typedef std::vector<VisitCbkT>	CbkVectorT;
		VisitorTypeStub(uint32 _tid = 0xffffffff):tid(_tid){}
		uint32			tid;
		CbkVectorT		cbkvec;
	};
	
	struct ObjectStub{
		ObjectStub(Object *_pobj = NULL):pobj(_pobj), uid(0){}
		Object	*pobj;
		uint32	uid;
	};
	
public:
	Service():crtobjtypeid(0), pmtx(NULL){
		pmtx = new Mutex;
		registerObjectType<Object, Service>();
		registerVisitorType<Object, Visitor>();
	}
	
	template <class O>
	ObjectUidT insert(O *_po, const ObjectUidT &_ruid = invalid_uid()){
		Mutex::Locker		lock(mutex());
		const uint16		tid(objectTypeId<O>());
		ObjectTypeStub		&rots(objectTypeStub(tid));
		const IndexT 		objidx(doInsertObject(*_po, tid, _ruid));
		
		//static_cast<Object*>(_po)->idx = objidx;
		
		const ObjectUidT	objuid(make_object_uid(this->index(), objidx, uid(objidx)));
		
		(*rots.insert_callback)(_po, this, objuid);
		
		return objuid;
	}
	
	template <class O>
	O* object(const ObjectUidT &_ruid){
		Mutex::Locker		lock(mutex());
		const uint16		tid(objectTypeId<O>());
		Object 				*po(objectAt(_ruid.first, _ruid.second));
		if(po && po->tid == tid){
			return static_cast<O*>(po);
		}
		return NULL;
	}
	
	void erase(const Object &_robj);
	
	bool signal(ulong _sm);
	bool signal(ulong _sm, const ObjectUidT &_ruid);
	bool signal(ulong _sm, Object &_robj);
	bool signal(ulong _sm, IndexT _fullid, uint32 _uid);
	template <class I>
	bool signal(ulong _sm, I &_rbeg, const I &_rend){
		return false;
	}
	
	bool signal(DynamicSharedPointer<Signal> &_rsig);
	template <class I>
	bool signal(DynamicSharedPointer<Signal> &_rsig, I &_rbeg, const I &_rend){
		return false;
	}
	bool signal(DynamicPointer<Signal> &_rsig, Object &_robj);
	bool signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid);
	
	template <class V>
	void visit(V &_rv){
		
	}
	
	template <class V>
	void visit(V &_rv, const ObjectUidT &_ruid){
		if(is_invalid_uid(_ruid)){
			return;
		}
		int	 			visidx(-1);
		
		for(int i(vistpvec.size() - 1); i >= 0; --i){
			if(V::isType(vistpvec[i].tid)){
				visidx = i;
				break;
			}
		}
		if(visidx < 0) return;
		
		IndexT			idx(compute_index(_ruid.first));
		Mutex::Locker	lock(mutex(idx));
		Object			*po(objectAt(idx, _ruid.second));
		
		doVisit(po, _rv, visidx);
	}
	
	template <class V, class I>
	void visit(V &_rv, const I &_rbeg, const I &_rend){
		
	}
	
	Mutex& mutex(){
		return *pmtx;
	}
protected:
	Mutex& mutex(IndexT	_idx);
	void insertObject(Object &_ro, const ObjectUidT &_ruid);
	void eraseObject(const Object &_ro);
	
	uint32 uid(const IndexT _idx)const{
		return objvec[_idx].uid;
	}
	
	template <class O, class S>
	void registerObjectType(){
		const uint32	objtpid(objectTypeId<O>());
		ObjectTypeStub &rts(safe_at(objtpvec, objtpid));
		rts.erase_callback = &erase_cbk<O, S>;
		rts.insert_callback = &insert_cbk<O, S>;
	}
	template <class O, class V>
	void registerVisitorType(){
		const uint32	objtpid(objectTypeId<O>());
		const uint32	vistpid(V::staticTypeId());
		int				pos(-1);
		for(VisitorTypeStubVectorT::const_iterator it(vistpvec.begin()); it != vistpvec.end(); ++it){
			if(it->tid == vistpid){
				pos = it - vistpvec.begin();
				break;
			}
		}
		if(pos < 0){
			pos = vistpvec.size();
			vistpvec.push_back(VisitorTypeStub(vistpid));
		}
		VisitorTypeStub	&rvts(vistpvec[pos]);
		safe_at(rvts.cbkvec, objtpid) = &visit_cbk<O, V>;
	}
private:
	ObjectTypeStub& objectTypeStub(uint _tid){
		if(_tid >= objtpvec.size()) _tid = 0;
		return objtpvec[_tid];
	}
	template <class O>
	uint objectTypeId(){
		static const uint v(newObjectTypeId());
		return v;
	}
	IndexT index(){
		return 0;
	}
	IndexT doInsertObject(Object &_ro, uint16 _tid,const ObjectUidT &_ruid);
	uint newObjectTypeId(){
		return crtobjtypeid++;
	}
	Object* objectAt(const IndexT &_ridx, uint32 _uid){
		if(objvec[_ridx].uid == _uid){
			return objvec[_ridx].pobj;
		}
		return NULL;
	}
	void doVisit(Object *_po, Visitor &_rv, uint32 _visidx);
private:
	typedef std::vector<ObjectStub>			ObjectStubVectorT;
	typedef std::vector<ObjectTypeStub>		ObjectTypeStubVectorT;
	typedef std::vector<VisitorTypeStub>	VisitorTypeStubVectorT;
	ObjectStubVectorT		objvec;
	ObjectTypeStubVectorT	objtpvec;
	VisitorTypeStubVectorT	vistpvec;
	uint					crtobjtypeid;
	Mutex					*pmtx;
};

void Service::insertObject(Object &_ro, const ObjectUidT &_ruid){
	
}
void Service::eraseObject(const Object &_ro){
	
}

IndexT Service::doInsertObject(Object &_ro, uint16 _tid, const ObjectUidT &_ruid){
	_ro.typeId(_tid);
	IndexT idx(objvec.size());
	objvec.push_back(ObjectStub());
	objvec.back().pobj = &_ro;
	_ro.idx = idx;
	return idx;
}

void Service::erase(const Object &_robj){
	ObjectTypeStub	&rots(objectTypeStub(_robj.typeId()));
	
	(*rots.erase_callback)(_robj, this);
	
	ObjectStub		&ros(objvec[_robj.idx]);
	
	ros.pobj = NULL;
	++ros.uid;
}

void Service::doVisit(Object *_po, Visitor &_rv, uint32 _visidx){
	if(!_po) return;
	VisitorTypeStub	&rvts(vistpvec[_visidx]);
	if(_po->typeId() < rvts.cbkvec.size() && rvts.cbkvec[_po->typeId()] != NULL){
		(*rvts.cbkvec[_po->typeId()])(_po, _rv);
	}else{
		Service::visit_cbk<Object, Visitor>(_po, _rv);
	}
}

class FirstObject;
class SecondObject;

struct AVisitor: public Dynamic<AVisitor, Visitor>{
	virtual void visitObject(FirstObject &_robj) = 0;
	virtual void visitObject(SecondObject &_robj) = 0;
};

class AService: public Dynamic<AService, Service>{
public:
	AService(){
		registerObjectType<FirstObject, AService>();
		registerObjectType<SecondObject, AService>();
		registerVisitorType<FirstObject, AVisitor>();
		registerVisitorType<SecondObject, AVisitor>();
	}
	void insertObject(FirstObject &_ro, const ObjectUidT &_ruid);
	void insertObject(SecondObject &_ro, const ObjectUidT &_ruid);
	
	void eraseObject(const FirstObject &_ro);
	void eraseObject(const SecondObject &_ro);
};


void AService::insertObject(FirstObject &_ro, const ObjectUidT &_ruid){
	
}
void AService::insertObject(SecondObject &_ro, const ObjectUidT &_ruid){
	
}

void AService::eraseObject(const FirstObject &_ro){
	
}
void AService::eraseObject(const SecondObject &_ro){
	
}


class ThirdObject;

struct BVisitor: Dynamic<BVisitor, AVisitor>{
	virtual void visitObject(ThirdObject &_robj) = 0;
};

class BService: public Dynamic<BService, AService>{
public:
    BService(){
		registerObjectType<FirstObject, BService>();
		registerObjectType<ThirdObject, BService>();
		registerVisitorType<ThirdObject, BVisitor>();
	}
	
	void insertObject(FirstObject &_ro, const ObjectUidT &_ruid);
	void insertObject(ThirdObject &_ro, const ObjectUidT &_ruid);
	
	void eraseObject(const FirstObject &_ro);
	void eraseObject(const ThirdObject &_ro);
};

void BService::insertObject(FirstObject &_ro, const ObjectUidT &_ruid){
	
}
void BService::insertObject(ThirdObject &_ro, const ObjectUidT &_ruid){
	
}

void BService::eraseObject(const FirstObject &_ro){
	
}
void BService::eraseObject(const ThirdObject &_ro){
	
}

struct FirstObject: Object{
	
};

struct SecondObject: Object{
	
};

struct ThirdObject: Object{
	
};


int main(){
	std::vector<int>	v;
	
	//v.insert(v.begin() + 10, 1);
	safe_at(v, 10) = 1;
	
	for(vector<int>::const_iterator it(v.begin()); it != v.end(); ++it){
		cout<<*it<<' ';
	}
	cout<<endl;
	AService as;
	BService bs;
	
	FirstObject		*po1_1(new FirstObject);
	FirstObject		*po1_2(new FirstObject);
	SecondObject	*po2_1(new SecondObject);
	SecondObject	*po2_2(new SecondObject);
	ThirdObject		*po3_1(new ThirdObject);
	ThirdObject		*po3_2(new ThirdObject);
	
	ObjectUidT fuid(as.insert(po1_1));
	ObjectUidT suid(as.insert(po2_1));
	as.insert(po3_1);
	
	FirstObject *pfo(as.object<FirstObject>(fuid));
	SecondObject *pso(as.object<SecondObject>(suid));
	cassert(pfo == po1_1);
	cassert(pso == po2_1);
	
	bs.insert(po1_2);
	bs.insert(po2_2);
	bs.insert(po3_2);
	
	
	
	as.erase(*po1_1);
	as.erase(*po2_1);
	as.erase(*po3_1);
	
	bs.erase(*po1_2);
	bs.erase(*po2_2);
	bs.erase(*po3_2);
	
	return 0;
}
