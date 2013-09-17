// serialization/src/binary.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <vector>

#include "system/common.hpp"
#include "system/exception.hpp"

#ifdef HAS_CPP11
#include <unordered_map>
#else
#include <map>
#endif

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "serialization/idtypemapper.hpp"

namespace solid{
namespace serialization{
//================================================================
struct TypeMapperBase::Data{
	struct FunctionStub{
		FunctionStub(
			FncSerT _pfs = NULL,
			FncDesT _pfd = NULL,
			const char *_name = NULL,
			const uint32 _id = 0
		):pfs(_pfs), pfd(_pfd), name(_name), id(_id){}
		
		FncSerT 	pfs;
		FncDesT 	pfd;
		const char 	*name;
		uint32		id;
	};
	struct PointerHash{
		size_t operator()(const char* const & _p)const{
			return reinterpret_cast<size_t>(_p);
		}
	};
	struct PointerCmpEqual{
		bool operator()(const char * const &_p1, const char * const &_p2)const{
			return _p1 == _p2;
		}
	};
	struct PointerCmpLess{
		bool operator()(const char * const &_p1, const char * const &_p2)const{
			return _p1 < _p2;
		}
	};
	typedef std::deque<FunctionStub>	FunctionVectorT;
#ifdef HAS_CPP11
	typedef std::unordered_map<const char *, size_t, PointerHash, PointerCmpEqual>		FunctionStubMapT;
#else
	typedef std::map<const char*, size_t, PointerCmpLess>								FunctionStubMapT;
#endif

	Data():crtpos(1){
		fncvec.push_back(FunctionStub(NULL, NULL, NULL, 1));
	}
	~Data();
	FunctionVectorT		fncvec;
	FunctionStubMapT	fncmap;
	size_t				crtpos;
};
//================================================================
TypeMapperBase::Data::~Data(){
	
}
TypeMapperBase::TypeMapperBase():d(*(new Data())){
}

TypeMapperBase::~TypeMapperBase(){
	delete &d;
}

TypeMapperBase::FncSerT TypeMapperBase::function(const uint32 _id, uint32* &_rpid)const{
	//Locker<Mutex>				lock(d.mtx);
	const Data::FunctionStub	&rfs(d.fncvec[_id]);
	
	_rpid = const_cast<uint32*>(&rfs.id);
	
	return rfs.pfs;
}
TypeMapperBase::FncSerT TypeMapperBase::function(const char *_pid, uint32* &_rpid)const{
	//Locker<Mutex>				lock(d.mtx);
	const Data::FunctionStub	&rfs(d.fncvec[d.fncmap[_pid]]);
	
	_rpid = const_cast<uint32*>(&rfs.id);
	
	return rfs.pfs;
}
TypeMapperBase::FncDesT TypeMapperBase::function(const uint32 _id)const{
	//Locker<Mutex>				lock(d.mtx);
	const CRCValue<uint32>		crcval(CRCValue<uint32>::check_and_create(_id));
	const uint32				idx(crcval.value());
	if(idx < d.fncvec.size()){
		const Data::FunctionStub	&rfs(d.fncvec[idx]);
		if(_id == rfs.id){
			return rfs.pfd;
		}
	}
	return NULL;
}

TypeMapperBase::FncDesT TypeMapperBase::function(const uint16 _id)const{
	//Locker<Mutex>				lock(d.mtx);
	const CRCValue<uint16>		crcval(CRCValue<uint16>::check_and_create(_id));
	const uint16				idx(crcval.value());
	if(idx < d.fncvec.size()){
		const Data::FunctionStub	&rfs(d.fncvec[idx]);
		if(_id == rfs.id){
			return rfs.pfd;
		}
	}
	return NULL;
}

TypeMapperBase::FncDesT TypeMapperBase::function(const uint8  _id)const{
	//Locker<Mutex>				lock(d.mtx);
	const CRCValue<uint8>		crcval(CRCValue<uint8>::check_and_create(_id));
	const uint8					idx(crcval.value());
	if(idx < d.fncvec.size()){
		const Data::FunctionStub	&rfs(d.fncvec[idx]);
		if(_id == rfs.id){
			return rfs.pfd;
		}
	}
	return NULL;
}

uint32 TypeMapperBase::insertFunction(FncSerT _fs, FncDesT _fd, uint32 _pos, const char *_name){
	//Locker<Mutex> lock(d.mtx);
	if(!_pos){
		while(d.crtpos < d.fncvec.size() && d.fncvec[d.crtpos].pfs != NULL){
			++d.crtpos;
		}
		if(d.crtpos == d.fncvec.size()){
			d.fncvec.push_back(Data::FunctionStub());
		}
		_pos = d.crtpos;
		++d.crtpos;
	}else{
		if(_pos >= d.fncvec.size()){
			d.fncvec.resize(_pos + 1);
		}
		if(d.fncvec[_pos].pfs){
			THROW_EXCEPTION_EX("Overlapping identifiers", _pos);
			return _pos;
		}
	}
	
	CRCValue<uint32>	crcval(_pos);
	
	if(!crcval.ok()){
		THROW_EXCEPTION_EX("Invalid CRCValue", _pos);
	}
	
	Data::FunctionStub	&rfs(d.fncvec[_pos]);
	rfs.pfs = _fs;
	rfs.pfd = _fd;
	rfs.name = _name;
	rfs.id = (uint32)crcval;
	if(d.fncmap.find(_name) == d.fncmap.end()){
		d.fncmap[rfs.name] = _pos;
	}
	return _pos;
}

uint32 TypeMapperBase::insertFunction(FncSerT _fs, FncDesT _fd, uint16 _pos, const char *_name){
	//Locker<Mutex> lock(d.mtx);
	if(!_pos){
		while(d.crtpos < d.fncvec.size() && d.fncvec[d.crtpos].pfs != NULL){
			++d.crtpos;
		}
		if(d.crtpos == d.fncvec.size()){
			d.fncvec.push_back(Data::FunctionStub());
		}
		_pos = d.crtpos;
		++d.crtpos;
	}else{
		if(_pos >= d.fncvec.size()){
			d.fncvec.resize(_pos + 1);
		}
		if(d.fncvec[_pos].pfs){
			THROW_EXCEPTION_EX("Overlapping identifiers", _pos);
			return _pos;
		}
	}
	
	CRCValue<uint16>	crcval(_pos);
	
	if(!crcval.ok()){
		THROW_EXCEPTION_EX("Invalid CRCValue", _pos);
	}
	
	Data::FunctionStub	&rfs(d.fncvec[_pos]);
	rfs.pfs = _fs;
	rfs.pfd = _fd;
	rfs.name = _name;
	rfs.id = (uint16)crcval;
	if(d.fncmap.find(_name) == d.fncmap.end()){
		d.fncmap[rfs.name] = _pos;
	}
	return _pos;
}

uint32 TypeMapperBase::insertFunction(FncSerT _fs, FncDesT _fd, uint8  _pos, const char *_name){
	//Locker<Mutex> lock(d.mtx);
	if(!_pos){
		while(d.crtpos < d.fncvec.size() && d.fncvec[d.crtpos].pfs != NULL){
			++d.crtpos;
		}
		if(d.crtpos == d.fncvec.size()){
			d.fncvec.push_back(Data::FunctionStub());
		}
		_pos = d.crtpos;
		++d.crtpos;
	}else{
		if(_pos >= d.fncvec.size()){
			d.fncvec.resize(_pos + 1);
		}
		if(d.fncvec[_pos].pfs){
			THROW_EXCEPTION_EX("Overlapping identifiers", _pos);
			return _pos;
		}
	}
	
	CRCValue<uint8>		crcval(_pos);
	
	if(!crcval.ok()){
		THROW_EXCEPTION_EX("Invalid CRCValue", _pos);
	}
	
	Data::FunctionStub	&rfs(d.fncvec[_pos]);
	rfs.pfs = _fs;
	rfs.pfd = _fd;
	rfs.name = _name;
	rfs.id = (uint8)crcval;
	if(d.fncmap.find(_name) == d.fncmap.end()){
		d.fncmap[rfs.name] = _pos;
	}
	return _pos;
}

/*virtual*/ bool TypeMapperBase::prepareStorePointer(
	void *_pser, void *_p,
	uint32 _rid, const char *_name,
	void *_pctx
)const{
	return false;
}

/*virtual*/ bool TypeMapperBase::prepareStorePointer(
	void *_pser, void *_p,
	const char *_pid, const char *_name,
	void *_pctx
)const{
	return false;
}

/*virtual*/ bool TypeMapperBase::prepareParsePointer(
	void *_pdes, std::string &_rs,
	void *_p, const char *_name,
	void *_pctx,
	FncInitPointerT _pinicbk
)const{
	return false;
}
/*virtual*/ void TypeMapperBase::prepareParsePointerId(
	void *_pdes, std::string &_rs,
	const char *_name
)const{
	
}

}//namespace serialization
}//namespace solid