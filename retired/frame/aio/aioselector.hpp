// frame/aio/selector.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SELECTOR_HPP
#define SOLID_FRAME_AIO_SELECTOR_HPP

#include "system/timespec.hpp"
#include "frame/common.hpp"
#include "frame/selectorbase.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{

struct TimeSpec;

namespace frame{
namespace aio{

class Object;
class Socket;

typedef DynamicPointer<Object>	ObjectPointerT;

//! An asynchronous IO selector to be used with the template SelectPool
/*!
	A selector will help SelectPool to actively hold aio::objects.
	A selector must export a certain interface requested by the SelectPool,
	and the pool will have one for its every thread.
*/
class Selector: public frame::SelectorBase{
public:
	typedef ObjectPointerT		JobT;
	typedef Object				ObjectT;
	
	Selector();
	~Selector();
	bool init(ulong _cp);
	//signal a specific object
	void raise(uint32 _pos);
	void run();
	ulong capacity()const;
	ulong size() const;
	bool empty()const;
	bool full()const;
	
	bool push(JobT &_rcon);
	void prepare();
	void unprepare();
private:
	struct Stub;
	ulong doReadPipe();
	ulong doAllIo();
	ulong doFullScan();
	void doFullScanCheck(Stub &_rs, const ulong _pos);
	ulong doExecuteQueue();
	ulong doAddNewStub();
	
	void doUnregisterObject(Object &_robj, int _lastfailpos = -1);
	ulong doIo(Socket &_rsock, ulong _evs, ulong _filter = 0);
	ulong doExecute(const ulong _pos);
	void doPrepareObjectWait(const size_t _pos, const TimeSpec &_timepos);
private://data
	struct Data;
	Data	&d;
};


}//namespace aio
}//namespace frame
}//namespace solid

#endif

