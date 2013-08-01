// frame/requestuid.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_REQUESTUID_HPP
#define SOLID_FRAME_REQUESTUID_HPP

#include "frame/common.hpp"

namespace solid{
namespace frame{

//! Unique identifier for a request
struct RequestUid{
	RequestUid(
		IndexT  _objidx = 0,
		uint32 _objuid = 0,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	):objidx(_objidx), objuid(_objuid), reqidx(_reqidx), requid(_requid){
	}
	RequestUid(
		const ObjectUidT &_ruid,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	):objidx(_ruid.first), objuid(_ruid.second), reqidx(_reqidx), requid(_requid){
	}
	void set(
		IndexT  _objidx = 0,
		uint32 _objuid = 0,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	){
		objidx = _objidx;
		objuid = _objuid;
		reqidx = _reqidx;
		requid = _requid;
	}
	void set(
		const ObjectUidT &_ruid,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	){
		objidx = _ruid.first;
		objuid = _ruid.second;
		reqidx = _reqidx;
		requid = _requid;
	}
	IndexT	objidx;
	uint32	objuid;
	uint32	reqidx;
	uint32	requid;
};


struct SpecificRequestUid{
	class ForcedCreate;
	SpecificRequestUid(const ForcedCreate&);
	SpecificRequestUid(){}
	void prepareThread();
	void unprepareThread();
	RequestUid* operator->()const;
	RequestUid* get() const;
	RequestUid& operator*()const;
private:
	SpecificRequestUid(const SpecificRequestUid&);
	SpecificRequestUid& operator=(const SpecificRequestUid&);
};

extern SpecificRequestUid requestuidptr;

}//namespace frame
}//namespace solid


#endif
