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
	
	void unregister();
	bool isRegistered()const;
	virtual void doStop();
private:
	void id(const IndexT &_fullid);
	
	void stop();
private:
	struct NotificationStub{
		size_t			event;
		size_t			index;
		MessagePointerT msgptr;
	};
	
	typedef std::vector<NotificationStub>		NotificationVectorT;
	
	IndexT						fullid;
	CompletionHandler			*pcpllst;

	ATOMIC_NS::atomic<IndexT>	selidx;
	ATOMIC_NS::atomic<uint32>	seluid;
	
	NotificationVectorT			ntfvec;
	
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

}//namespace frame
}//namespace solid


#endif
