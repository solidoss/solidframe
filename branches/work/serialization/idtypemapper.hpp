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
};

template <class Ser, class Des, typename Int, typename Mtx = Mutex>
class IdTypeMapper: public TypeMapperBase{
	
	typedef IdTypeMapper<Ser, Des, Int, Mtx>	ThisT;
	typedef typename Ser::ContextT				SerContextT;
	typedef typename Des::ContextT				DesContextT;
	
	
	template <class T>
	static bool doMapSer(void *_p, void *_ps, void *_pid, const char *_name, void *_pctx){
		Ser &rs = *reinterpret_cast<Ser*>(_ps);
		T	&rt = *reinterpret_cast<T*>(_p);
		Int &rid = *reinterpret_cast<Int*>(_pid);
		
		rt & rs;
		rs.push(rid, _name);
		return true;
	}
	
	template <class T>
	static bool doMapDes(void *_p, void *_pd, const char *_name, void *_pctx){
		Des &rd	= *reinterpret_cast<Des*>(_pd);
		T*	&rpt = *reinterpret_cast<T**>(_p);
		rpt = new T;
		*rpt & rd;
		return true;
	}
	
	template <class T, class H>
	static bool doMapSerHandle(void *_p, void *_ps, void *_pid, const char *_name, void *_pctx){
		Ser				&rs = *reinterpret_cast<Ser*>(_ps);
		T				&rt = *reinterpret_cast<T*>(_p);
		Int				&rid = *reinterpret_cast<Int*>(_pid);
		SerContextT		*pctx = reinterpret_cast<SerContextT*>(_pctx);
		H				handle;
		
		if(handle.checkStore(&rt, pctx)){
			rt & rs;
			rs.push(rid, _name);
			return true;
		}else{
			return false;
		}
	}
	
	
	template <class T, class H>
	static bool doMapDesHandle(void *_p, void *_pd, const char *_name, void *_pctx){
		Des				&rd = *reinterpret_cast<Des*>(_pd);
		T*				&rpt = *reinterpret_cast<T**>(_p);
		DesContextT		*pctx = reinterpret_cast<DesContextT*>(_pctx);
		H				handle;
		
		if(handle.checkLoad(rpt, pctx)){
			rpt = new T;
			rd.template pushHandlePointer<T, H>(rpt, pctx, _name);
			*rpt & rd;
			return true;
		}else{
			return false;
		}
	}
	
	template <class T, class CT>
	static bool doMapDes(void *_p, void *_pd, const char *_name, void *_pctx){
		Des &rd = *reinterpret_cast<Des*>(_pd);
		T*  &rpt = *reinterpret_cast<T**>(_p);
		rpt = new T(CT());
		*rpt & rd;
		return true;
	}
	template <class T>
	static bool doMapDesSpecific(void *_p, void *_pd, const char *_name, void *_pctx){
		Des &rd	= *reinterpret_cast<Des*>(_pd);
		T*	&rpt = *reinterpret_cast<T**>(_p);
		rpt = Specific::template uncache<T>();
		*rpt & rd;
		return true;
	}
	template <class T, class CT>
	static bool doMapDesSpecific(void *_p, void *_pd, const char *_name, void *_pctx){
		Des &rd = *reinterpret_cast<Des*>(_pd);
		T*  &rpt = *reinterpret_cast<T**>(_p);
		rpt = Specific::template uncache<T>(CT());
		*rpt & rd;
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
			Locker<Mtx> lock(m);
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
			Locker<Mtx> lock(m);
			pf = this->function(_pid, pid);
		}
		return (*pf)(_pt, _pser, pid, _name, _pctx);
	}
	
	/*virtual*/ bool prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name,
		void *_pctx
	)const{
		const Int							&rid(*reinterpret_cast<const Int*>(_rs.data()));
		typename UnsignedType<Int>::Type	idx(rid);
		FncDesT								pf;
		{
			Locker<Mtx> lock(m);
			pf = this->function(idx);
		}
		if(pf){
			(*pf)(_p, _pdes, _name, _pctx);
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
