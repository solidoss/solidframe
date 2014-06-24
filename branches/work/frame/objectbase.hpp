// frame/objectbase.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_OBJECTBASE_HPP
#define SOLID_FRAME_OBJECTBASE_HPP

#include <vector>

#include "frame/message.hpp"
#include "frame/common.hpp"
#include "frame/event.hpp"
#include "utility/dynamictype.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

namespace solid{
namespace frame{

class Manager;
class Service;
class Message;
class SelectorBase;
class Object;
class CompletionHandler;

typedef DynamicPointer<Message>	MessagePointerT;

class ObjectBase: public Dynamic<ObjectBase, DynamicShared<> >{
public:
	static ObjectBase& specific();
	
	//! Get the id of the object
	IndexT id() const;
	
	//! Virtual destructor
	virtual ~ObjectBase();
	
protected:
	friend class Service;
	friend class Manager;
	friend class SelectorBase;
	
	//! Constructor
	ObjectBase();
	
	//! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
	size_t grabSignalMask(const size_t _leave = 0);
	
	void unregister();
	bool isRegistered()const;
	virtual void doStop();
	
	bool notify(const size_t _smask);
	bool notify(Event const &_revt);
private:
	void id(const IndexT &_fullid);
	
	void stop();
private:
	typedef std::vector<Event>		EventVectorT;
	
	IndexT						fullid;
	CompletionHandler			*pcpllst;

	ATOMIC_NS::atomic<IndexT>	selidx;
	ATOMIC_NS::atomic<uint32>	seluid;
	ATOMIC_NS::atomic<size_t>	smask;
	
	EventVectorT				eventvec;
	
};

inline IndexT ObjectBase::id()	const {
	return fullid;
}

inline bool ObjectBase::isRegistered()const{
	return fullid != static_cast<IndexT>(-1);
}

inline void ObjectBase::id(IndexT const &_fullid){
	fullid = _fullid;
}

inline size_t ObjectBase::grabSignalMask(const size_t _leave = 0){
	return smask.fetch_and(_leave/*, ATOMIC_NS::memory_order_seq_cst*/);
}

inline bool ObjectBase::notify(const size_t _smask){
	const size_t osm = smask.fetch_or(_smask/*, ATOMIC_NS::memory_order_seq_cst*/);
	return _smask != osm;; 
}

inline bool ObjectBase::notify(Event const &_revt){
	if(is_valid_index(selidx.fetch(/*ATOMIC_NS::memory_order_seq_cst*/))){
		return true;
	}else{
		eventvec.push_back(_revt);
		return false;
	}
}

}//namespace frame
}//namespace solid


#endif
