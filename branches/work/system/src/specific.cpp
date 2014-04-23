// system/src/specific.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <stack>
#include <vector>
#include "system/common.hpp"
#include "system/cassert.hpp"
#include "system/specific.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/exception.hpp"

#ifndef OBJ_CACHE_CAP
#define OBJ_CACHE_CAP 4096*2
#endif

namespace solid{

struct BufferNode{
	BufferNode *pnext;
};

#ifdef HAS_SAFE_STATIC
static const unsigned specificPosition(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}
#else
const unsigned specificPositionStub(){
	static const unsigned	thrspecpos = Thread::specificId();
	return thrspecpos;
}

void once_cbk(){
	specificPositionStub();
}

const unsigned specificPosition(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk, once);
	return specificPositionStub();
}

#endif
//static unsigned		stkid = 0;
struct CleaningVector: std::vector<Specific::FncT>{
	CleaningVector(){
		this->reserve(OBJ_CACHE_CAP);
	}
};

static CleaningVector	cv;

//This is what is holded on a thread
struct SpecificData{
	enum{
		Capacity = 13
	};
	struct CachePoint{
		CachePoint():pnode(0), cp(0), sz(0){}
		BufferNode		*pnode;
		uint32			cp;
		uint32			sz;
	};
	struct ObjectCachePoint{
		ObjectCachePoint():pnode(NULL),cp(0), sz(0){}
		BufferNode		*pnode;
		uint32			cp;
		uint32			sz;
	};
	
	typedef std::vector<ObjectCachePoint>	ObjCachePointVecT;
	
	SpecificData(SpecificCacheControl *_pcc);
	
	~SpecificData();
	
	inline static SpecificData& current(){
		return *reinterpret_cast<SpecificData*>(Thread::specific(specificPosition()));
	}
	
	CachePoint 				cps[Capacity];
	ObjCachePointVecT		ops;
	SpecificCacheControl	*pcc;
};

struct BasicCacheControl:SpecificCacheControl{
	/*virtual*/ bool release();
};

//----------------------------------------------------------------------------------------------------
void SpecificObject::operator delete (void *_pv, std::size_t _sz){
	int id = Specific::sizeToIndex(_sz);
	if(id >= 0){
		char * pc = (char*)_pv;
		Specific::pushBuffer(pc, id);
	}else{
		delete []reinterpret_cast<char*>(_pv);
	}
}
void* SpecificObject::operator new (std::size_t _sz){
	int id = Specific::sizeToIndex(_sz);
	if(id >= 0){
		return Specific::popBuffer(id);
	}else{
		return new char[_sz];
	}
}
//----------------------------------------------------------------------------------------------------
// 4096 * 4096 per every id
//TODO: make it configurable
unsigned SpecificCacheControl::stackCapacity(unsigned _bufid)const{
	return 4096 * 4096 / (1 << _bufid);
}
bool BasicCacheControl::release(){
	return false;//do not delete this object
}

static inline BufferNode* bufferNodePointer(void *_p){
	char *p = reinterpret_cast<char*>(_p);
	return reinterpret_cast<BufferNode*>(p - sizeof(BufferNode));
}

static inline void* voidPointer(BufferNode *_pv){
	char *p(reinterpret_cast<char*>(_pv));
	return p + sizeof(BufferNode);
}
char* checkObjectBuffer(void *_p){
	char *p = reinterpret_cast<char*>(_p);
	p -= sizeof(BufferNode);
	void **pv = reinterpret_cast<void**>(p);
	if(*pv != reinterpret_cast<void*>(p)){
		THROW_EXCEPTION_EX("Array Bounds Write or Caching a non unchached object ", _p);
	}
	return p;
}
//****************************************************************************************************
//		SpecificData
//****************************************************************************************************
SpecificData::SpecificData(SpecificCacheControl *_pcc):pcc(_pcc){
	ops.reserve(OBJ_CACHE_CAP);
}
SpecificData::~SpecificData(){
	if(pcc->release()) delete pcc;
	idbgx(Debug::specific, "destroy all cached buffers");
	for(int i(0); i < Capacity; ++i){
		vdbgx(Debug::specific, i<<" cp = "<<cps[i].cp<<" sz = "<<cps[i].sz<<" specific_id = "<<Specific::sizeToIndex((1<<i)));
		
		BufferNode	*pbn(cps[i].pnode);
		BufferNode	*pnbn;
		uint32 		cnt(0);
		while(pbn){
			pnbn = pbn->pnext;
			delete []reinterpret_cast<char*>(pbn);
			++cnt;
			pbn = pnbn;
		}
		
		if(cnt != cps[i].sz || cnt != cps[i].cp){
			THROW_EXCEPTION_EX("Memory leak specific buffers ", i);
		}
	}
	
	idbgx(Debug::specific, "destroy all cached objects");
	Locker<Mutex> lock(Thread::gmutex());
	for(ObjCachePointVecT::iterator it(ops.begin()); it != ops.end(); ++it){
		vdbgx(Debug::specific, "it->cp = "<<it->cp);
		BufferNode	*pbn(it->pnode);
		BufferNode	*pnbn;
		uint32 		cnt(0);
		while(pbn){
			pnbn = pbn->pnext;
			void **pv = reinterpret_cast<void**>(pbn);
			*pv = pbn;
			++cnt;
			(*cv[it - ops.begin()])(voidPointer(pbn));
			
			pbn = pnbn;
		}
		if(cnt != it->sz || cnt != it->cp){
			THROW_EXCEPTION_EX("Memory leak specific objects ", (int)(it - ops.begin()));
		}
	}
}
//****************************************************************************************************
//		Specific
//****************************************************************************************************
static BasicCacheControl bcc;
void destroy(void *_pv){
	SpecificData *psd = reinterpret_cast<SpecificData*>(_pv);
	delete psd;
}
/*static*/ void Specific::prepareThread(SpecificCacheControl *_pcc){
	if(!_pcc) _pcc = static_cast<SpecificCacheControl*>(&bcc);
	Thread::specific(specificPosition(), new SpecificData(_pcc), &destroy);
}
//----------------------------------------------------------------------------------------------------
/*static*/ int Specific::sizeToIndex(unsigned _sz){
	if(_sz <= (1 * sizeof(BufferNode))){
		return 0;
	}else if(_sz <= 2 * sizeof(BufferNode)){
		return 1;
	}else if(_sz <= 4 * sizeof(BufferNode)){
		return 2;
	}else if(_sz <= 8 * sizeof(BufferNode)){
		return 3;
	}else if(_sz <= 16 * sizeof(BufferNode)){
		return 4;
	}else if(_sz <= 32 * sizeof(BufferNode)){
		return 5;
	}else if(_sz <= 64 * sizeof(BufferNode)){
		return 6;
	}else if(_sz <= 128 * sizeof(BufferNode)){
		return 7;
	}else if(_sz <= 256 * sizeof(BufferNode)){
		return 8;
	}else if(_sz <= 512 * sizeof(BufferNode)){
		return 9;
	}else if(_sz <= 1024 * sizeof(BufferNode)){
		return 10;
	}else if(_sz <= 2048 * sizeof(BufferNode)){
		return 11;
	}else if(_sz <= 4096 * sizeof(BufferNode)){
		return 12;
	}
	return -1;
}
/*static*/ int Specific::capacityToIndex(unsigned _sz){
	switch(_sz){
		case (1 * sizeof(BufferNode)):		return 0;
		case (2 * sizeof(BufferNode)):		return 1;
		case (4 * sizeof(BufferNode)):		return 2;
		case (8 * sizeof(BufferNode)):		return 3;
		case (16 * sizeof(BufferNode)):		return 4;
		case (32 * sizeof(BufferNode)):		return 5;
		case (64 * sizeof(BufferNode)):		return 6;
		case (128 * sizeof(BufferNode)):	return 7;
		case (256 * sizeof(BufferNode)):	return 8;
		case (512 * sizeof(BufferNode)):	return 9;
		case (1024 * sizeof(BufferNode)):	return 10;
		case (2048 * sizeof(BufferNode)):	return 11;
		case (4096 * sizeof(BufferNode)):	return 12;
		default:return -1;
	}
}

/*static*/ unsigned Specific::indexToCapacity(unsigned _id){
	static const uint cps[] = {
		sizeof(BufferNode),
		2 * sizeof(BufferNode),
		4 * sizeof(BufferNode),
		8 * sizeof(BufferNode),
		16 * sizeof(BufferNode),
		32 * sizeof(BufferNode),
		64 * sizeof(BufferNode),
		128 * sizeof(BufferNode),
		256 * sizeof(BufferNode),
		512 * sizeof(BufferNode),
		1024 * sizeof(BufferNode),
		2048 * sizeof(BufferNode),
		4096 * sizeof(BufferNode)
	};
	return cps[_id];
}

/*static*/ char* Specific::popBuffer(unsigned _id){
	cassert(_id < SpecificData::Capacity);
	SpecificData				&rsd(SpecificData::current());
	SpecificData::CachePoint	&rcp(rsd.cps[_id]);
	char *tb;
	if(rcp.pnode){
		BufferNode *pnbn = rcp.pnode->pnext;
		tb = reinterpret_cast<char*>(rcp.pnode);
		rcp.pnode = pnbn;
		--rcp.sz;
	}else{
		++rcp.cp;
		tb = new char[indexToCapacity(_id)];
	}
	idbgx(Debug::specific,"popBuffer "<<_id<<" cp "<<rcp.cp<<' '<<(void*)tb);
	return tb;
}
/*static*/ void Specific::pushBuffer(char *&_pb, unsigned _id){
	cassert(_pb);
	cassert(_id < SpecificData::Capacity);
	SpecificData				&rsd(SpecificData::current());
	SpecificData::CachePoint	&rcp(rsd.cps[_id]);
	idbgx(Debug::specific,"pushBuffer "<<_id<<" cp "<<rcp.cp<<' '<<(void*)_pb);
	if(rcp.sz < rsd.pcc->stackCapacity(_id)){
		BufferNode *pbn(reinterpret_cast<BufferNode*>(_pb));
		++rcp.sz;
		pbn->pnext = rcp.pnode;
		rcp.pnode = pbn;
	}else{
		--rcp.cp;
		delete []_pb;
	}
	_pb = NULL;
}
//----------------------------------------------------------------------------------------------------
//for caching objects
/*static*/ unsigned Specific::stackid(FncT _pf){
	Locker<Mutex> lock(Thread::gmutex());
	SpecificData &rsd(SpecificData::current());
	cassert(rsd.ops.size() < rsd.ops.capacity());
	cv.push_back(_pf);
	return cv.size() - 1;
}

/*static*/ int Specific::push(void *_pv, unsigned _id, unsigned _maxcp){
	SpecificData					&rsd(SpecificData::current());
	cassert(_pv);
	cassert(_id < rsd.ops.size());
	SpecificData::ObjectCachePoint	&rocp(rsd.ops[_id]);
	if(rocp.sz < _maxcp){
		checkObjectBuffer(_pv);
		BufferNode *pbn(bufferNodePointer(_pv));
		pbn->pnext = rocp.pnode;
		rocp.pnode = pbn;
		++rocp.sz;
		return 0;
	}
	--rsd.ops[_id].cp;
	return -1;
}
/*static*/ void* Specific::pop(unsigned _id){
	SpecificData &rsd(SpecificData::current());
	if(_id >= rsd.ops.size()){
		rsd.ops.resize(_id + 1);
	}else if(rsd.ops[_id].pnode){
		BufferNode	*pbn(rsd.ops[_id].pnode);
		void 		*pv(voidPointer(pbn));
		rsd.ops[_id].pnode = pbn->pnext;
		--rsd.ops[_id].sz;
		void **ppv = reinterpret_cast<void**>(pbn);
		*ppv = pbn;
		return pv;
	}
	++rsd.ops[_id].cp;
	return NULL;
}

/*static*/ void* Specific::doAllocateBuffer(size_t _sz){
	_sz += sizeof(BufferNode);
	char *p = new char[_sz];
	void **pv = reinterpret_cast<void**>(p);
	*pv = p;
	return p + sizeof(BufferNode);
}

/*static*/ void Specific::doDeallocateBuffer(void *_p){
	delete []checkObjectBuffer(_p);
}

}//namespace solid
