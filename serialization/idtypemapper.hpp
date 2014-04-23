// serialization/idtypemapper.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_TYPE_MAPPER_HPP
#define SOLID_SERIALIZATION_TYPE_MAPPER_HPP

#include <string>
#include <typeinfo>
#include "system/specific.hpp"
#include "system/mutex.hpp"
#include "utility/common.hpp"
#include "serialization/typemapperbase.hpp"

namespace solid{
namespace serialization{

struct FakeMutex{
	void lock(){}
	void unlock(){}
	void sharedLock(){}
	void sharedUnlock(){}
};

template <class Ser, class Des, typename Int, typename Mtx = SharedMutex>
class IdTypeMapper: public TypeMapperBase{
	
	typedef IdTypeMapper<Ser, Des, Int, Mtx>	ThisT;
	typedef typename Ser::ContextT				SerContextT;
	typedef typename Des::ContextT				DesContextT;
	
	
	template <class T>
	static bool doMapSer(void *_p, void *_ps, void *_pid, const char *_name, void */*_pctx*/){
		Int			&rid = *reinterpret_cast<Int*>(_pid);
		Ser			&rs = *reinterpret_cast<Ser*>(_ps);
		T			&rp = *reinterpret_cast<T*>(_p);
		rs.push(rp, _name);
		rs.push(rid, _name);
		return true;
	}
	
	template <class T>
	static bool doMapDes(void *_p, void *_pd, const char *_name, void */*_pctx*/, FncInitPointerT _pinicbk){
		Des		&rd	= *reinterpret_cast<Des*>(_pd);
		T		*pt = new T;
		if(_pinicbk){
			(*_pinicbk)(_p, pt);
		}else{
			T*		&rpt = *reinterpret_cast<T**>(_p);
			rpt = pt;
		}
		
		T		&rp = *pt;
		rd.push(rp, _name);
		
		return true;
	}
	
	template <class T, class H>
	static bool doMapSerHandle(void *_p, void *_ps, void *_pid, const char *_name, void *_pctx){
		Ser				&rs = *reinterpret_cast<Ser*>(_ps);
		T				&rp = *reinterpret_cast<T*>(_p);
		Int				&rid = *reinterpret_cast<Int*>(_pid);
		H				handle;
		SerContextT		&rctx = *reinterpret_cast<SerContextT*>(_pctx);
		
		if(handle.checkStore(&rp, rctx)){
			rs.template pushHandlePointer<T, H>(&rp, _name);
			rs.push(rp, _name);
			handle.beforeSerialization(rs, &rp, rctx);//DO NOT Move this line - must be before storing the id
			rs.push(rid, _name);
			return true;
		}else{
			return false;
		}
	}
	
	
	template <class T, class H>
	static bool doMapDesHandle(void *_p, void *_pd, const char *_name, void *_pctx, FncInitPointerT _pinicbk){
		Des				&rd = *reinterpret_cast<Des*>(_pd);
		//T*				&rpt = *reinterpret_cast<T**>(_p);
		T				*pt = NULL;
		H				handle;
		DesContextT		&rctx = *reinterpret_cast<DesContextT*>(_pctx);
		
		if(handle.checkLoad(pt, rctx)){
			pt = new T;
			if(_pinicbk){
				(*_pinicbk)(_p, pt);
			}else{
				T*		&rpt = *reinterpret_cast<T**>(_p);
				rpt = pt;
			}
			T		&rp = *pt;
			rd.template pushHandlePointer<T, H>(pt, _name);
			rd.push(rp, _name);
			handle.beforeSerialization(rd, &rp, rctx);
			return true;
		}else{
			return false;
		}
	}
	
	template <class T, class CT>
	static bool doMapDes(void *_p, void *_pd, const char *_name, void */*_pctx*/, FncInitPointerT _pinicbk){
		Des		&rd	= *reinterpret_cast<Des*>(_pd);
		T		*pt = new T(CT());
		if(_pinicbk){
			(*_pinicbk)(_p, pt);
		}else{
			T*		&rpt = *reinterpret_cast<T**>(_p);
			rpt = pt;
		}
		
		T		&rp = *pt;
		rd.push(rp, _name);
		return true;
	}
	template <class T>
	static bool doMapDesSpecific(void *_p, void *_pd, const char *_name, void */*_pctx*/, FncInitPointerT _pinicbk){
		Des		&rd	= *reinterpret_cast<Des*>(_pd);
		T		*pt = Specific::template uncache<T>();
		if(_pinicbk){
			(*_pinicbk)(_p, pt);
		}else{
			T*		&rpt = *reinterpret_cast<T**>(_p);
			rpt = pt;
		}
		
		T		&rp = *pt;
		rd.push(rp, _name);
		return true;
	}
	template <class T, class CT>
	static bool doMapDesSpecific(void *_p, void *_pd, const char *_name, void */*_pctx*/, FncInitPointerT _pinicbk){
		Des		&rd	= *reinterpret_cast<Des*>(_pd);
		T		*pt = Specific::template uncache<T>(CT());
		
		if(_pinicbk){
			(*_pinicbk)(_p, pt);
		}else{
			T*		&rpt = *reinterpret_cast<T**>(_p);
			rpt = pt;
		}
		
		T		&rp = *pt;
		rd.push(rp, _name);
		return true;
	}
public:
	IdTypeMapper(){}
	template <class T>
	uint32 insert(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		Locker<Mtx>		lock(m);
		return this->insertFunction(&ThisT::template doMapSer<T>, &ThisT::template doMapDes<T>, idx, typeid(T).name());
	}
	template <class T, typename CT>
	uint32 insert(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		Locker<Mtx>		lock(m);
		return this->insertFunction(&ThisT::template doMapSer<T>, &ThisT::template doMapDes<T, CT>, idx, typeid(T).name());
	}
	template <class T, class H>
	uint32 insertHandle(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		Locker<Mtx>		lock(m);
		return this->insertFunction(&ThisT::template doMapSerHandle<T, H>, &ThisT::template doMapDesHandle<T, H>, idx, typeid(T).name());
	}
	
	template <class T>
	uint32 insertSpecific(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		Locker<Mtx>		lock(m);
		return this->insertFunction(&ThisT::template doMapSer<T>, &ThisT::template doMapDesSpecific<T>, idx, typeid(T).name());
	}
	template <class T, typename CT>
	uint32 insertSpecific(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		Locker<Mtx>		lock(m);
		return this->insertFunction(&ThisT::template doMapSer<T>, &ThisT::template doMapDesSpecific<T, CT>, idx, typeid(T).name());
	}
	uint32 realIdentifier(uint32 _idx)const{
		return _idx;
	}
private:
	/*virtual*/ bool prepareStorePointer(
		void *_pser, void *_pt,
		uint32 _id, const char *_name,
		void *_pctx
	)const{
		uint32	*pid(NULL);
		FncSerT	pf; 
		{
			SharedLocker<Mtx> lock(m);
			pf = this->function(_id, pid);
		}
		return (*pf)(_pt, _pser, pid, _name, _pctx);
	}
	/*virtual*/ bool prepareStorePointer(
		void *_pser, void *_pt,
		const char *_pid, const char *_name,
		void *_pctx
	)const{
		uint32	*pid(NULL);
		FncSerT	pf; 
		{
			SharedLocker<Mtx> lock(m);
			pf = this->function(_pid, pid);
		}
		return (*pf)(_pt, _pser, pid, _name, _pctx);
	}
	
	/*virtual*/ bool prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name,
		void *_pctx,
		FncInitPointerT _pinicbk
	)const{
		const Int							&rid(*reinterpret_cast<const Int*>(_rs.data()));
		typename UnsignedType<Int>::Type	idx(rid);
		FncDesT								pf;
		{
			SharedLocker<Mtx> lock(m);
			pf = this->function(idx);
		}
		if(pf){
			(*pf)(_p, _pdes, _name, _pctx, _pinicbk);
			return true;
		}else{
			return false;
		}
	}
	/*virtual*/ void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	)const{
		Des		&rd(*reinterpret_cast<Des*>(_pdes));
		Int		&rid(*reinterpret_cast<Int*>(const_cast<char*>(_rs.data())));
		rd.push(rid, _name);
	}
private:
	mutable Mtx		m;
};

}//namespace serialization
}//namespace solid

#endif
