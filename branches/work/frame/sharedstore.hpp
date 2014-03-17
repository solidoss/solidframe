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
#include <vector>
#include <deque>

namespace solid{
namespace frame{
namespace shared{

struct PointerBase;

class StoreBase: public Dynamic<StoreBase, Object>{
public:
	Manager& manager();
	virtual ~StoreBase();
protected:
	enum Kind{
		UniqueE,
		SharedE
	};
	
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
	
	typedef std::vector<UidT>					UidVectorT;
	typedef std::vector<size_t>					SizeVectorT;
	typedef std::vector<ExecWaitStub>			ExecWaitVectorT;
	
	StoreBase(Manager &_rm);
	Mutex &mutex();
	Mutex &mutex(const size_t _idx);
	
	size_t doAllocateIndex();
	void pointerId(PointerBase &_rpb, UidT const & _ruid);
	void doExecuteCache();
	void doCacheObjectIndex(const size_t _idx);
private:
	friend struct PointerBase;
	void erasePointer(UidT const & _ruid, const bool _isalive);
	virtual bool doDecrementObjectUseCount(UidT const &_uid, const bool _isalive) = 0;
	virtual void doExecute(UidVectorT const &_ruidvec, SizeVectorT &_ridxvec, ExecWaitVectorT &_rexewaitvec) = 0;
	virtual void doResizeObjectVector(const size_t _newsz) = 0;
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
protected:
	PointerBase(StoreBase *_psb = NULL):psb(_psb){}
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
	explicit Pointer(T *_pt, StoreBase *_psb, UidT const &_uid): PointerBase(_psb, _uid), pt(_pt){}
	
	Pointer(PointerT const &_rptr):PointerBase(_rptr), pt(_rptr.release()){}
	~Pointer(){
		clear();
	}
	
	PointerT operator=(PointerT const &_rptr){
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
		
	}
	
	PointerT	insertShared(T &_rt){
	
	}
	
	PointerT	insertUnique(T &_rt){
		
	}
	
	PointerT	insertAlive(){
		
	}
	
	PointerT	insertShared(){
		
	}
	
	PointerT	insertUnique(){
		
	}
	
	//Try get an alive pointer for an intem
	PointerT	alive(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//Try get an unique pointer for an item
	PointerT	unique(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//Try get a shared pointer for an item
	PointerT	shared(UidT const & _ruid, ERROR_NS::error_code &_rerr){
		
	}
	
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestReinit(F &_f, size_t _flags = 0){
		PointerT				uniptr(this);
		size_t					idx = -1;
		ERROR_NS::error_code	err;
		{
			Locker<Mutex>	lock(this->mutex());
			
			if(!_f.prepareIndex(controller(), idx, _flags, err)){
				//an index was not found - need to allocate one
				idx = this->doAllocateIndex();
			}
			if(!err){
				Locker<Mutex>	lock(this->mutex(idx));
				uniptr = doTryGetUnique(idx);
				if(uniptr.empty()){
					doPushWait(_f, StoreBase::UniqueE);
				}else if(!_f.preparePointer(controller(), uniptr, _flags, err)){
					cassert(uniptr.empty());
					doPushWait(_f, StoreBase::UniqueE);
				}
			}
		}
		if(!uniptr.empty() || err){
			_f(controller(), uniptr, err);
			return true;
		}
		return false;
	}

	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestShared(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	//! Return true if the _f was called within the current thread
	template <typename F>
	bool requestUnique(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	//! Return true if the _f was called within the current thread
	//_f will be called uniquely when object's alive count is zero
	template <typename F>
	bool requestReinit(F _f, UidT const & _ruid, const size_t _flags = 0){
		return false;
	}
	
	ControllerT	&controller(){
		return *static_cast<ControllerT*>(this);
	}
private:
	
	struct WaitStub{
		WaitStub():kind(StoreBase::StoreBase::ReinitWaitE){}
		StoreBase::WaitKind		kind;
		WaitStub 				*pnext;
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
	
	PointerT doTryGetUnique(const size_t _idx){
		return PointerT();
	}
	template <typename F>
	void doPushWait(F &_f, const StoreBase::Kind _k){
		
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
	/*virtual*/ void doExecute(
		StoreBase::UidVectorT const &_ruidvec,
		StoreBase::SizeVectorT &_ridxvec,
		StoreBase::ExecWaitVectorT &_rexewaitvec
	){
		Mutex *pmtx = &this->mutex(_ruidvec.front().first);
		pmtx->lock();
		for(StoreBase::UidVectorT::const_iterator it(_ruidvec.begin()); it != _ruidvec.end(); ++it){
			Mutex *ptmpmtx = &this->mutex(it->first);
			if(pmtx != ptmpmtx){
				pmtx->unlock();
				pmtx = ptmpmtx;
				pmtx->lock();
			}
			Stub	&rs = stubvec[it->first];
			if(it->second == rs.uid){
				if(rs.canClear()){
					_ridxvec.push_back(it->first);
				}else{
					doExecute(it->first, _rexewaitvec);
				}
			}
		}
		pmtx->unlock();
		//now execute "waits"
		for(StoreBase::ExecWaitVectorT::const_iterator it(_rexewaitvec.begin()); it != _rexewaitvec.end(); ++it){
			PointerT	ptr(reinterpret_cast<T*>(it->pt), this, it->uid);
			WaitStub	&rw = *reinterpret_cast<WaitStub*>(it->pw);
			//TODO:
		}
		
		{
			Locker<Mutex>	lock(this->mutex());
			if(_ridxvec.size()){
				pmtx = &this->mutex(_ridxvec.front());
				pmtx->lock();
				for(StoreBase::SizeVectorT::const_iterator it(_ridxvec.begin()); it != _ridxvec.end(); ++it){
					Mutex *ptmpmtx = &this->mutex(*it);
					if(pmtx != ptmpmtx){
						pmtx->unlock();
						pmtx = ptmpmtx;
						pmtx->lock();
					}
					Stub	&rs = stubvec[*it];
					if(rs.canClear()){
						rs.clear();
						controller().clear(rs.obj, *it);
						StoreBase::doCacheObjectIndex(*it);
					}
				}
				pmtx->unlock();
			}
			StoreBase::doExecuteCache();
		}
	}
	void doExecute(const size_t _idx, StoreBase::ExecWaitVectorT &_rexewaitvec){
		Stub		&rs = stubvec[_idx];
		WaitStub	*pwait = rs.pwaitfirst;
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
			_rexewaitvec.push_back(StoreBase::ExecWaitStub(UidT(_idx, rs.uid), &rs.obj, pwait));
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
}//namespace shared
}//namespace frame
}//namespace solid

#endif
