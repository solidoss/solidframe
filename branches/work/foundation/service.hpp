/* Declarations file service.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_SERVICE_HPP
#define FOUNDATION_SERVICE_HPP

#include "foundation/object.hpp"
#include "foundation/objectpointer.hpp"
#include "system/mutex.hpp"
#include <vector>

namespace foundation{

class Manager;

//! A visitor structure for the basic service
struct Visitor: Dynamic<Visitor>{
	virtual void visitObject(Object &_robj) = 0;
};

//! Static container of objects
/*!
	<b>Overview:</b><br>
	The service statically keeps objects providing capabilities to
	signal explicit objects, signal all objects and visit objects.
	
	It also provides a mutex for every contained object.
	
	Services can be started and stopped but they cannot be destroyed.
	
	Stopping means signalling all sub-objects with S_KILL, and wait
	for all objects to unregister.
	
	Also a foundation::Service is a foundation::Object meaning
	that it resides within an active container.
	
*/
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
		static const EraseCbkT	default_erase_cbk;
		static const InsertCbkT	default_insert_cbk;
		ObjectTypeStub(
			EraseCbkT _pec = default_erase_cbk,
			InsertCbkT _pic = default_insert_cbk
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
	
	typedef std::vector<ObjectTypeStub>		ObjectTypeStubVectorT;
	typedef std::vector<VisitorTypeStub>	VisitorTypeStubVectorT;
	ObjectTypeStubVectorT	objtpvec;
	VisitorTypeStubVectorT	vistpvec;
public:
	static void dynamicRegister();
	Service(bool _started = true, int _objpermutbts = 6, int _mutrowsbts = 8, int _mutcolsbts = 8);
	
	/*virtual*/ ~Service();
	
	template <typename O>
	ObjectUidT insert(O *_po, const IndexT &_ridx = invalid_uid().first){
		Mutex::Locker		lock(serviceMutex());
		const uint			tid(O::staticTypeId());
		ObjectTypeStub		&rots(objectTypeStub(tid));
		const ObjectUidT	objuid(doInsertObject(*_po, tid, _ridx));
		
		(*rots.insert_callback)(_po, this, objuid);
		
		return objuid;
	}
	
	template <typename S, class O>
	ObjectUidT insert(ObjectPointer<O> &_op, uint _schidx = 0, const IndexT &_ridx = invalid_uid().first){
		Mutex::Locker		lock(serviceMutex());
		const uint			tid(O::staticTypeId());
		ObjectTypeStub		&rots(objectTypeStub(tid));
		const ObjectUidT	objuid(doInsertObject(*_op, tid, _ridx));
		
		(*rots.insert_callback)(_op.ptr(), this, objuid);
		
		S::schedule(_op, _schidx);
		
		return objuid;
	}
	
	IndexT size()const;
	
	void erase(const Object &_robj);
	
	bool signal(DynamicPointer<foundation::Signal> &_sig);
	
	bool signal(ulong _sm);
	bool signal(ulong _sm, const ObjectUidT &_ruid);
	bool signal(ulong _sm, IndexT _fullid, uint32 _uid);
	bool signal(ulong _sm, const Object &_robj);

	template <class I>
	bool signal(ulong _sm, I &_rbeg, const I &_rend){
		return false;
	}
	
	bool signal(DynamicSharedPointer<Signal> &_rsig);
	bool signal(DynamicPointer<Signal> &_rsig, const ObjectUidT &_ruid);
	bool signal(DynamicPointer<Signal> &_rsig, IndexT _fullid, uint32 _uid);
	bool signal(DynamicPointer<Signal> &_rsig, const Object &_robj);
	
	template <class I>
	bool signal(DynamicSharedPointer<Signal> &_rsig, I &_rbeg, const I &_rend){
		return false;
	}
	
	template <class V>
	void visit(V &_rv){
		int	 			visidx(-1);
		
		for(int i(this->vistpvec.size() - 1); i >= 0; --i){
			if(V::isType(vistpvec[i].tid)){
				visidx = i;
				break;
			}
		}
		if(visidx < 0) return false;
		
		return doVisit(_rv, visidx);
	}
	
	template <class V>
	bool visit(V &_rv, const ObjectUidT &_ruid){
		if(is_invalid_uid(_ruid)){
			return false;
		}
		int	 			visidx(-1);
		
		for(int i(vistpvec.size() - 1); i >= 0; --i){
			if(V::isType(vistpvec[i].tid)){
				visidx = i;
				break;
			}
		}
		if(visidx < 0) return false;
		
		return doVisit(_rv, visidx, _ruid);
	}
	
	template <class V, class I>
	void visit(V &_rv, const I &_rbeg, const I &_rend){
		
	}
	
	Mutex& mutex(const Object &_robj);
	uint32 uid(const Object &_robj)const;
	uint32 uid(const IndexT &_idx)const;
	
	int start(bool _wait = true);
	int stop(bool _wait = true);
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
	
	virtual void dynamicExecute(DynamicPointer<> &_dp);
protected:
	Mutex &serviceMutex()const;
	void insertObject(Object &_ro, const ObjectUidT &_ruid);
	void eraseObject(const Object &_ro);
	void expectedCount(const IndexT &_rcnt);
	const IndexT& expectedCount()const;
	
	template <typename O>
	ObjectUidT insertLockless(O *_po, const IndexT &_ridx = invalid_uid().first){
		const uint			tid(O::staticTypeId());
		ObjectTypeStub		&rots(objectTypeStub(tid));
		const ObjectUidT	objuid(doInsertObject(*_po, tid, _ridx));
		
		(*rots.insert_callback)(_po, this, objuid);
		
		return objuid;
	}
	
	template <typename S, class O>
	ObjectUidT insertLockless(ObjectPointer<O> &_op, uint _schidx = 0, const IndexT &_ridx = invalid_uid().first){
		const uint			tid(O::staticTypeId());
		ObjectTypeStub		&rots(objectTypeStub(tid));
		const ObjectUidT	objuid(doInsertObject(*_op, tid, _ridx));
		
		(*rots.insert_callback)(_op.ptr(), this, objuid);
		
		S::schedule(_op, _schidx);
		
		return objuid;
	}
	
	template <typename O, class S>
	static void registerObjectType(S *_ps){
		const uint32	objtpid(O::staticTypeId());
		Service			&rs(*static_cast<Service*>(_ps));
		ObjectTypeStub &rts(safe_at(rs.objtpvec, objtpid));
		rts.erase_callback = &erase_cbk<O, S>;
		rts.insert_callback = &insert_cbk<O, S>;
	}
	
	template <typename O, typename V>
	void registerVisitorType(){
		const uint32	objtpid(O::staticTypeId());
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
	Mutex& mutex(const IndexT &_ridx);
	Object* objectAt(const IndexT &_ridx, uint32 _uid);
	Object* objectAt(const IndexT &_ridx);
	virtual int doStart(ulong _evs, TimeSpec &_rtout);
	virtual int doRun(ulong _evs, TimeSpec &_rtout);
	virtual int doStop(ulong _evs, TimeSpec &_rtout);
private:
	friend class Manager;
	ObjectTypeStub& objectTypeStub(uint _tid){
		if(_tid >= objtpvec.size()) _tid = 0;
		return objtpvec[_tid];
	}
	ObjectUidT doInsertObject(Object &_ro, uint32 _tid, const IndexT &_ruid);
	void doVisit(Object *_po, Visitor &_rv, uint32 _visidx);
	const Service& operator=(const Service &);
	bool doSignalAll(ulong _sm);
	bool doSignalAll(DynamicSharedPointer<Signal> &_rsig);
	bool doVisit(Visitor &_rv, uint _visidx);
	bool doVisit(Visitor &_rv, uint _visidx, const ObjectUidT &_ruid);
	//this is called by manager 
	void invalidateService();
	/*virtual*/ void init(Mutex *);
protected:
	typedef DynamicExecuter<void, Service>	DynamicExecuterT;
	DynamicExecuterT		de;
private:
	struct Data;
	Data					&d;
};

}//namespace foundation

#endif
