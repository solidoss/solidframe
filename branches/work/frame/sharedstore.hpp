// frame/sharedstore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_SHARED_STORE_HPP
#define SOLID_FRAME_SHARED_STORE_HPP

#include "frame/common.hpp"
#include "frame/object.hpp"
#include "utility/dynamictype.hpp"
#include "system/error.hpp"
#include "system/mutex.hpp"
#include "system/function.hpp"
#include <vector>
#include <deque>

namespace solid{
namespace frame{
namespace shared{

struct PointerBase;

typedef std::vector<UidT>					UidVectorT;



class StoreBase: public Dynamic<StoreBase, Object>{
public:
	typedef shared::UidVectorT					UidVectorT;
	struct Accessor{
		StoreBase& store()const{
			return rs;
		}
		Mutex& mutex(){
			return rs.mutex();
		}
		UidVectorT& consumeEraseVector()const{
			return rs.consumeEraseVector();
		}
		void notify(ulong _sm);
	private:
		friend class StoreBase;
		StoreBase	&rs;
		Accessor(StoreBase &_rs):rs(_rs){}
		Accessor& operator=(const Accessor&);
	};
	Manager& manager();
	
	virtual ~StoreBase();
protected:
	enum WaitKind{
		UniqueWaitE,
		SharedWaitE,
		ReinitWaitE
	};
	enum StubStates{
		UnlockedStateE = 1,
		UniqueLockStateE,
		SharedLockStateE,
	};
	
	struct ExecWaitStub{
		ExecWaitStub():pt(NULL), pw(NULL){}
		ExecWaitStub(UidT const & _uid, void *_pt, void *_pw):uid(_uid), pt(_pt), pw(_pw){}
		
		UidT	uid;
		void	*pt;
		void	*pw;
	};
	
	typedef std::vector<size_t>					SizeVectorT;
	typedef std::vector<ExecWaitStub>			ExecWaitVectorT;
	
	StoreBase(Manager &_rm);
	Mutex &mutex();
	Mutex &mutex(const size_t _idx);
	
	size_t doAllocateIndex();
	void* doTryAllocateWait();
	void pointerId(PointerBase &_rpb, UidT const & _ruid);
	void doExecuteCache();
	void doCacheObjectIndex(const size_t _idx);
	size_t atomicMaxCount()const;
	
	UidVectorT& consumeEraseVector()const;
	UidVectorT& fillEraseVector()const;
	
	SizeVectorT& indexVector()const;
	ExecWaitVectorT& executeWaitVector()const;
	Accessor accessor();
	void notifyObject(UidT const & _ruid);
private:
	friend struct PointerBase;
	void erasePointer(UidT const & _ruid, const bool _isalive);
	virtual bool doDecrementObjectUseCount(UidT const &_uid, const bool _isalive) = 0;
	virtual bool doExecute() = 0;
	virtual void doResizeObjectVector(const size_t _newsz) = 0;
	virtual void doExecuteOnSignal(ulong _sm) = 0;
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	struct Data;
	Data &d;
};

struct PointerBase{
	const UidT& id()const{
		return uid;
	}
	bool empty()const{
		return is_invalid_uid(id());
	}
	StoreBase* store()const{
		return psb;
	}
	Mutex* mutex()const{
		if(!empty()){
			return &psb->mutex(uid.first);
		}else return NULL;
	}
protected:
	PointerBase(StoreBase *_psb = NULL):uid(invalid_uid()), psb(_psb){}
	PointerBase(StoreBase *_psb, UidT const &_uid):uid(_uid), psb(_psb){}
	PointerBase(PointerBase const &_rpb):uid(_rpb.uid), psb(_rpb.psb){}
	
	void doClear(const bool _isalive);
	void doReset(StoreBase *_psb, UidT const &_uid)const{
		uid = _uid;
		psb = _psb;
	}
private:
	friend class StoreBase;
	mutable UidT		uid;
	mutable StoreBase	*psb;
};

template <
	class T
>
struct Pointer: PointerBase{
	
	typedef Pointer<T>		PointerT;
	
	Pointer(StoreBase *_psb = NULL):PointerBase(_psb), pt(NULL){}
	explicit Pointer(
		T *_pt,
		StoreBase *_psb  = NULL,
		UidT const &_uid = invalid_uid()
	): PointerBase(_psb, _uid), pt(_pt){}
	
	Pointer(PointerT const &_rptr):PointerBase(_rptr), pt(_rptr.release()){}
	~Pointer(){
		clear();
	}
	
	PointerT& operator=(PointerT const &_rptr){
		clear();
		doReset(_rptr.store(), _rptr.id());
		pt = _rptr.release();
		return *this;
	}
	
	void reset(T *_pt, StoreBase *_psb, UidT const &_uid){
		clear();
		pt = _pt;
		
	}
	bool alive()const{
		return pt == NULL;
	}
	
	T& operator*()const			{return *pt;}
	T* operator->()const		{return pt;}
	T* get() const				{return pt;}
	//operator bool () const	{return psig;}
	bool operator!()const		{return empty();}
	
	T* release()const{
		T *p = pt;
		pt = NULL;
		doReset(NULL, invalid_uid());
		return p;
	}
	void clear(){
		if(!empty()){
			doClear(alive());
			pt = NULL;
		}
	}
private:
	mutable T	*pt;
};


enum Flags{
	SynchronousTryFlag = 1
};

template <
	class T,
	class Ctl
>
class Store: public Dynamic<Store<T, Ctl>, StoreBase>{
public:
	typedef Pointer<T>							PointerT;
	typedef Ctl									ControllerT;
	typedef Dynamic<Store<T, Ctl>, StoreBase>	BaseT;
	
	Store(Manager &_rm):BaseT(_rm){}
	
	PointerT	insertAlive(T &_rt){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			rs.obj = _rt;
			ptr = doTryGetAlive(idx);
		}
		return ptr;
	}
	
	PointerT	insertShared(T &_rt){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			rs.obj = _rt;
			ptr = doTryGetShared(idx);
		}
		return ptr;
	}
	
	PointerT	insertUnique(T &_rt){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			rs.obj = _rt;
			ptr = doTryGetUnique(idx);
		}
		return ptr;
	}
	
	PointerT	insertAlive(){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			ptr = doTryGetAlive(idx);
		}
		return ptr;
	}
	
	PointerT	insertShared(){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			ptr = doTryGetShared(idx);
		}
		return ptr;
	}
	
	PointerT	insertUnique(){
		Locker<Mutex>	lock(this->mutex());
		const size_t	idx = this->doAllocateIndex();
		PointerT		ptr;
		{
			Locker<Mutex>	lock2(this->mutex(idx));
			ptr = doTryGetUnique(idx);
		}
		return ptr;
	}
	
	//Try get an alive pointer for an intem
	PointerT	alive(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		PointerT		ptr;
		const size_t	idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetAlive(idx);
			}
		}
		return ptr;
	}
	
	//Try get an unique pointer for an item
	PointerT	unique(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		PointerT		ptr;
		const size_t	idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetUnique(idx);
			}
		}
		return ptr;
	}
	
	//Try get a shared pointer for an item
	PointerT	shared(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		PointerT		ptr;
		const size_t	idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetShared(idx);
			}
		}
		return ptr;
	}
	
	bool uniqueToShared(PointerT const &_rptr){
		bool rv = false;
		bool do_notify = false;
		if(!_rptr.empty()){
			const size_t	idx = _rptr.id().first;
			if(idx < this->atomicMaxCount()){
				Locker<Mutex>	lock2(this->mutex(idx));
				Stub			&rs = stubvec[idx];
				if(rs.uid == _rptr.id().second){
					if(doSwitchUniqueToShared(idx)){
						if(rs.pwaitfirst && rs.pwaitfirst->kind == StoreBase::SharedWaitE){
							do_notify = true;
						}
						rv = true;
					}
				}
			}
		}
		if(do_notify){
			StoreBase::notifyObject(_rptr.id());
		}
		return rv;
	}
	
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestReinit(F &_f, size_t _flags = 0){
		PointerT				ptr(this);
		size_t					idx = -1;
		ERROR_NS::error_code	err;
		StoreBase::Accessor		acc = StoreBase::accessor();
		{
			Locker<Mutex>	lock(this->mutex());
			bool			found = controller().prepareIndex(acc, _f, idx, _flags, err);
			if(!found && !err){
				//an index was not found - need to allocate one
				idx = this->doAllocateIndex();
			}
			if(!err){
				Locker<Mutex>	lock(this->mutex(idx));
				ptr = doTryGetReinit(idx);
				if(ptr.empty()){
					doPushWait(idx, _f, StoreBase::ReinitWaitE);
				}else if(!controller().preparePointer(acc, _f, ptr, _flags, err)){
					cassert(ptr.empty());
					doPushWait(idx, _f, StoreBase::ReinitWaitE);
				}
			}
		}
		if(!ptr.empty() || err){
			_f(controller(), ptr, err);
			return true;
		}
		return false;
	}

	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestShared(F _f, UidT const & _ruid, const size_t _flags = 0){
		PointerT				ptr;
		ERROR_NS::error_code	err;
		const size_t			idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetShared(idx);
				if(ptr.empty()){
					doPushWait(idx, _f, StoreBase::SharedWaitE);
				}
			}else{
				err.assign(1, err.category());//TODO:
			}
		}else{
			err.assign(1, err.category());//TODO:
		}
		if(!ptr.empty() || err){
			_f(controller(), ptr, err);
			return true;
		}
		return false;
	}
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestUnique(F _f, UidT const & _ruid, const size_t _flags = 0){
		PointerT				ptr;
		ERROR_NS::error_code	err;
		const size_t			idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetUnique(idx);
				if(ptr.empty()){
					doPushWait(idx, _f, StoreBase::UniqueWaitE);
				}
			}else{
				err.assign(1, err.category());//TODO:
			}
		}else{
			err.assign(1, err.category());//TODO:
		}
		if(!ptr.empty() || err){
			_f(controller(), ptr, err);
			return true;
		}
		return false;
	}
	//! Return true if the _f was called within the current thread
	//_f will be called uniquely when object's alive count is zero
	template <typename F>
	bool requestReinit(F _f, UidT const & _ruid, const size_t _flags = 0){
		PointerT				ptr;
		ERROR_NS::error_code	err;
		const size_t			idx = _ruid.first;
		if(idx < this->atomicMaxCount()){
			Locker<Mutex>	lock2(this->mutex(idx));
			Stub			&rs = stubvec[idx];
			if(rs.uid == _ruid.second){
				ptr = doTryGetReinit(idx);
				if(ptr.empty()){
					doPushWait(idx, _f, StoreBase::ReinitWaitE);
				}
			}else{
				err.assign(1, err.category());//TODO:
			}
		}else{
			err.assign(1, err.category());//TODO:
		}
		if(!ptr.empty() || err){
			_f(controller(), ptr, err);
			return true;
		}
		return false;
	}
	
	ControllerT	&controller(){
		return *static_cast<ControllerT*>(this);
	}
private:
	typedef FUNCTION_NS::function<void(ControllerT&, PointerT&, ERROR_NS::error_code const&)>	FunctionT;
	struct WaitStub{
		WaitStub():kind(StoreBase::ReinitWaitE), pnext(NULL){}
		StoreBase::WaitKind		kind;
		WaitStub 				*pnext;
		FunctionT				fnc;
	};
	struct Stub{
		Stub(
		):uid(0), alivecnt(0), usecnt(0), state(StoreBase::UnlockedStateE), pwaitfirst(NULL), pwaitlast(NULL){}
		
		void clear(){
			++uid;
			state = StoreBase::UnlockedStateE;
		}
		bool canClear()const{
			return usecnt == 0 && alivecnt == 0 && pwaitfirst == NULL;
		}
		
		T			obj;
		uint32		uid;
		size_t		alivecnt;
		size_t		usecnt;
		uint8		state;
		WaitStub	*pwaitfirst;
		WaitStub	*pwaitlast;
	};
	
	typedef std::deque<Stub>		StubVectorT;
	typedef std::deque<WaitStub>	WaitDequeT;
	
	
	/*virtual*/ void doResizeObjectVector(const size_t _newsz){
		stubvec.resize(_newsz);
	}
	
	PointerT doTryGetAlive(const size_t _idx){
		Stub		&rs = stubvec[_idx];
		++rs.alivecnt;
		rs.state = StoreBase::UniqueLockStateE;
		return PointerT(NULL, this, UidT(_idx, rs.uid));
	}
	
	PointerT doTryGetShared(const size_t _idx){
		Stub		&rs = stubvec[_idx];
		if(rs.state == StoreBase::SharedLockStateE && rs.pwaitfirst == NULL){
			++rs.usecnt;
			return PointerT(&rs.obj, this, UidT(_idx, rs.uid));
		}
		return PointerT();
	}
	
	PointerT doTryGetUnique(const size_t _idx){
		Stub		&rs = stubvec[_idx];
		if(rs.usecnt == 0){
			cassert(rs.pwaitfirst == NULL);
			++rs.usecnt;
			rs.state = StoreBase::UniqueLockStateE;
			return PointerT(&rs.obj, this, UidT(_idx, rs.uid));
		}
		return PointerT();
	}
	PointerT doTryGetReinit(const size_t _idx){
		Stub		&rs = stubvec[_idx];
		if(rs.usecnt == 0 && rs.alivecnt == 0 && rs.pwaitfirst == NULL){
			++rs.usecnt;
			rs.state = StoreBase::UniqueLockStateE;
			return PointerT(&rs.obj, this, UidT(_idx, rs.uid));
		}
		return PointerT();
	}
	bool doSwitchUniqueToShared(const size_t _idx){
		Stub		&rs = stubvec[_idx];
		
		if(rs.state == StoreBase::UniqueLockStateE){
			cassert(rs.usecnt == 1);
			rs.state = StoreBase::SharedLockStateE;
			return true;
		}
		return false;
	}
	template <typename F>
	void doPushWait(const size_t _idx, F &_f, const StoreBase::WaitKind _k){
		Stub		&rs = stubvec[_idx];
		WaitStub	*pwait = reinterpret_cast<WaitStub*>(this->doTryAllocateWait());
		if(pwait == NULL){
			waitdq.push_back(WaitStub());
			pwait = &waitdq.back();
		}
		pwait->kind = _k;
		pwait->fnc = _f;
		pwait->pnext = NULL;
		
		if(rs.pwaitlast == NULL){
			rs.pwaitfirst = rs.pwaitlast = pwait;
		}else{
			rs.pwaitlast->pnext = pwait;
			rs.pwaitlast = pwait;
		}
	}
	
	/*virtual*/ bool doDecrementObjectUseCount(UidT const &_uid, const bool _isalive){
		//the coresponding mutex is already locked
		Stub &rs = stubvec[_uid.first];
		if(rs.uid == _uid.second){
			if(_isalive){
				--rs.alivecnt;
				return rs.usecnt == 0 && rs.alivecnt == 0;
			}else{
				--rs.usecnt;
				return rs.usecnt == 0;
			}
		}
		return false;
	}
	
	/*virtual*/ void doExecuteOnSignal(ulong _sm){
		StoreBase::Accessor			acc = StoreBase::accessor();
		controller().executeOnSignal(acc, _sm);
	}
	
	/*virtual*/ bool doExecute(){
		StoreBase::Accessor			acc = StoreBase::accessor();
		StoreBase::UidVectorT const &reraseuidvec = StoreBase::consumeEraseVector();
		StoreBase::SizeVectorT 		&rcacheidxvec = StoreBase::indexVector();
		StoreBase::ExecWaitVectorT 	&rexewaitvec = StoreBase::executeWaitVector();
		const size_t				eraseuidvecsize = reraseuidvec.size();
		bool						must_reschedule = controller().executeBeforeErase(acc);
		
		if(reraseuidvec.empty()){
			return must_reschedule;
		}
		Mutex						*pmtx = &this->mutex(reraseuidvec.front().first);
		pmtx->lock();
		for(StoreBase::UidVectorT::const_iterator it(reraseuidvec.begin()); it != reraseuidvec.end(); ++it){
			Mutex *ptmpmtx = &this->mutex(it->first);
			if(pmtx != ptmpmtx){
				pmtx->unlock();
				pmtx = ptmpmtx;
				pmtx->lock();
			}
			Stub	&rs = stubvec[it->first];
			if(it->second == rs.uid){
				if((it - reraseuidvec.begin()) >= eraseuidvecsize){
					//its an uid added by executeBeforeErase
					--rs.usecnt;
				}
				if(rs.canClear()){
					rcacheidxvec.push_back(it->first);
				}else{
					doExecuteErase(it->first);
				}
			}
		}
		pmtx->unlock();
		//now execute "waits"
		for(StoreBase::ExecWaitVectorT::const_iterator it(rexewaitvec.begin()); it != rexewaitvec.end(); ++it){
			PointerT				ptr(reinterpret_cast<T*>(it->pt), this, it->uid);
			WaitStub				&rw = *reinterpret_cast<WaitStub*>(it->pw);
			ERROR_NS::error_code	err;
			rw.fnc(this->controller(), ptr, err);
		}
		
		{
			Locker<Mutex>	lock(this->mutex());
			if(rcacheidxvec.size()){
				pmtx = &this->mutex(rcacheidxvec.front());
				pmtx->lock();
				for(StoreBase::SizeVectorT::const_iterator it(rcacheidxvec.begin()); it != rcacheidxvec.end(); ++it){
					Mutex	*ptmpmtx = &this->mutex(*it);
					if(pmtx != ptmpmtx){
						pmtx->unlock();
						pmtx = ptmpmtx;
						pmtx->lock();
					}
					Stub	&rs = stubvec[*it];
					if(rs.canClear()){
						rs.clear();
						must_reschedule = controller().clear(acc, rs.obj, *it) || must_reschedule;
						StoreBase::doCacheObjectIndex(*it);
					}
				}
				pmtx->unlock();
			}
			StoreBase::doExecuteCache();
		}
		return must_reschedule;
	}
	
	void doExecuteErase(const size_t _idx){
		Stub						&rs = stubvec[_idx];
		WaitStub					*pwait = rs.pwaitfirst;
		StoreBase::ExecWaitVectorT 	&rexewaitvec = StoreBase::executeWaitVector();
		while(pwait){
			switch(pwait->kind){
				case StoreBase::UniqueWaitE:
					if(rs.usecnt == 0){
						//We can deliver
						rs.state = StoreBase::UniqueLockStateE;
					}else{
						//cannot deliver right now - keep waiting
						return;
					}
					break;
				case StoreBase::SharedWaitE:
					if(rs.usecnt == 0 || rs.state == StoreBase::SharedLockStateE){
						rs.state = StoreBase::SharedLockStateE;
					}else{
						//cannot deliver right now - keep waiting
						return;
					}
					break;
				case StoreBase::ReinitWaitE:
					if(rs.usecnt == 0 && rs.alivecnt == 0){
						rs.state = StoreBase::UniqueLockStateE;
					}else{
						//cannot deliver right now - keep waiting
						return;
					}
					break;
				default:
					cassert(false);
					return;
			}
			++rs.usecnt;
			rexewaitvec.push_back(StoreBase::ExecWaitStub(UidT(_idx, rs.uid), &rs.obj, pwait));
			if(pwait != rs.pwaitlast){
				rs.pwaitfirst = pwait->pnext;
			}else{
				rs.pwaitlast = rs.pwaitfirst = NULL;
			}
			pwait = pwait->pnext;
		}
	}
private:
	StubVectorT		stubvec;
	WaitDequeT		waitdq;
};


inline void StoreBase::pointerId(PointerBase &_rpb, UidT const & _ruid){
	_rpb.uid = _ruid;
}
inline StoreBase::Accessor StoreBase::accessor(){
	return Accessor(*this);
}
}//namespace shared
}//namespace frame
}//namespace solid

#endif
