#include <iostream>
#include "system/common.hpp"
#include <vector>
#include "utility/dynamictype.hpp"
#include "system/mutex.hpp"

using namespace std;

typedef uint32 IndexT;

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

class Service;

struct Object{
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

struct Visitor{
	virtual void visitObject(Object &_robj) = 0;
};

class Service: public Dynamic<Service>{
	typedef bool (*TypeCbkT) (const uint32);
	typedef void (*EraseCbkT) (const Object &, Service *);
	typedef void (*InsertCbkT) (Object *, Service *, const ObjectUidT &);
	
	struct ObjectTypeStub{
		ObjectTypeStub():type_callback(NULL), erase_callback(NULL), insert_callback(NULL){}
		bool empty()const{
			return type_callback == NULL;
		}
		bool isType(const uint32 _id)const{
			return (*type_callback)(_id);
		}
		TypeCbkT	type_callback;
		EraseCbkT	erase_callback;
		InsertCbkT	insert_callback;
	};
	
	struct ObjectStub{
		ObjectStub(Object *_pobj = NULL):pobj(_pobj), uid(0){}
		Object	*pobj;
		uint32	uid;
	};
	template <class S>
	static bool type_cbk(const uint32 _id){
		return S::isType(_id);
	}
	template <class O, class S>
	static void insert_cbk(Object *_po, Service *_ps, const ObjectUidT &_ruid){
		static_cast<S*>(_ps)->insertObject(*static_cast<O*>(_po), _ruid);
	}
	template <class O, class S>
	static void erase_cbk(const Object &_ro, Service *_ps){
		static_cast<S*>(_ps)->eraseObject(static_cast<const O&>(_ro));
	}
	template <class O, class V>
	static void visit_cbk(Object *_po, Visitor *_pv){
		static_cast<V*>(_pv)->visitObject(static_cast<O&>(*_po));
	}
public:
	Service(){
		registerObjectType<Object, Service>();
		registerVisitorType<Object, Visitor>();
	}
	
	template <class O>
	ObjectUidT insert(O *_po, const ObjectUidT &_ruid = invalid_uid()){
		const uint16		tid(objectTypeId<O>());
		ObjectTypeStub		&rots(createObjectTypeStub(tid));
		const IndexT 		objidx(doInsertObject(*_po, tid, _ruid));
		
		static_cast<Object*>(_po)->idx = objidx;
		
		const ObjectUidT	objuid(make_object_uid(this->index(), objidx, uid(objidx)));
		
		(*rots.insert_callback)(_po, this, objuid);
		
		return objuid;
	}
	
	void erase(const Object &_robj);
	
	template <class V>
	void visit(V &_rv, const ObjectUidT &_ruid = invalid_uid()){
		if(is_valid_uid(_ruid)){
			
		}else{
			
		}
	}
	Mutex& mutex(){
		return *pmtx;
	}
protected:
	void insertObject(Object *_po, const ObjectUidT &_ruid);
	void eraseObject(const Object &_ro);
	
	uint32 uid(const IndexT _idx)const{
		return objvec[_idx].uid;
	}
	
	template <class O, class S>
	void registerObjectType(){
		
	}
	template <class O, class V>
	void registerVisitorType(){
		
	}
private:
	ObjectTypeStub& createObjectTypeStub(uint _tid){
		
	}
	ObjectTypeStub& objectTypeStub(uint _tid){
	}
	template <class O>
	uint objectTypeId(){
		static uint v(0);
		Mutex::Locker lock(mutex());
		return v++;
	}
	IndexT index(){
		return 0;
	}
	IndexT doInsertObject(Object &_ro, uint16 _tid,const ObjectUidT &_ruid);
private:
	typedef std::vector<ObjectStub>		ObjectStubVectorT;
	typedef std::vector<ObjectTypeStub>	ObjectTypeStubVectorT;
	ObjectStubVectorT		objvec;
	ObjectTypeStubVectorT	objtpvec;
	Mutex					*pmtx;
};

Service::ObjectTypeStub& Service::createObjectTypeStub(uint _tid){
	
}

void Service::insertObject(Object *_po, const ObjectUidT &_ruid){
	
}
void Service::eraseObject(const Object &_ro){
	
}

IndexT Service::doInsertObject(Object &_ro, uint16 _tid,const ObjectUidT &_ruid){
	
}

void Service::erase(const Object &_robj){
	ObjectTypeStub	&rots(objectTypeStub(_robj.typeId()));
	
	(*rots.erase_callback)(_robj, this);
	
	ObjectStub		&ros(objvec[_robj.idx]);
	
	ros.pobj = NULL;
	++ros.uid;
}

class FirstObject;
class SecondObject;

struct AVisitor: public Visitor{
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
	void insertObject(FirstObject *_po, const ObjectUidT &_ruid);
	void insertObject(SecondObject *_po, const ObjectUidT &_ruid);
	
	void eraseObject(const FirstObject &_ro);
	void eraseObject(const SecondObject &_ro);
};



class ThirdObject;

class BVisitor: public AVisitor{
	virtual void visitObject(ThirdObject &_robj) = 0;
};

class BService: public Dynamic<BService, AService>{
public:
    BService(){
		registerObjectType<FirstObject, BService>();
		registerObjectType<ThirdObject, BService>();
		registerVisitorType<ThirdObject, BVisitor>();
	}
	
	void insertObject(FirstObject *_po, const ObjectUidT &_ruid);
	void insertObject(ThirdObject *_po, const ObjectUidT &_ruid);
	
	void eraseObject(const FirstObject &_ro);
	void eraseObject(const ThirdObject &_ro);
};

struct FirstObject: Object{
	
};

struct SecondObject: Object{
	
};

struct ThirdObject: Object{
	
};


int main(){
	AService as;
	BService bs;
	
	FirstObject		*po1_1(new FirstObject);
	FirstObject		*po1_2(new FirstObject);
	SecondObject	*po2_1(new SecondObject);
	SecondObject	*po2_2(new SecondObject);
	ThirdObject		*po3_1(new ThirdObject);
	ThirdObject		*po3_2(new ThirdObject);
	
	as.insert(po1_1);
	as.insert(po2_1);
	as.insert(po3_1);
	
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