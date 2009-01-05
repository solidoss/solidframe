#ifndef REQUESTUID_HPP
#define REQUESTUID_HPP

#include "common.hpp"

namespace foundation{

//! Unique identifier for a request
struct RequestUid{
	RequestUid(
		ulong  _objidx = 0,
		uint32 _objuid = 0,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	):objidx(_objidx),
	objuid(_objuid),
	reqidx(_reqidx),
	requid(_requid){
	}
	void set(
		ulong  _objidx = 0,
		uint32 _objuid = 0,
		uint32 _reqidx = 0,
		uint32 _requid = 0
	){
		objidx = _objidx;
		objuid = _objuid;
		reqidx = _reqidx;
		requid = _requid;
	}
	ulong	objidx;
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
	RequestUid* ptr() const;
	RequestUid& operator*()const;
private:
	SpecificRequestUid(const SpecificRequestUid&);
	SpecificRequestUid& operator=(const SpecificRequestUid&);
};

extern SpecificRequestUid requestuidptr;

}


#endif
