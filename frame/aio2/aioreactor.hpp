// frame/aio/reactor.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_REACTOR_HPP
#define SOLID_FRAME_AIO_REACTOR_HPP

#include "system/timespec.hpp"
#include "frame/common.hpp"
#include "frame/reactorbase.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{

struct TimeSpec;

namespace frame{
namespace aio{

class Object;
class Socket;

typedef DynamicPointer<Object>	ObjectPointerT;

//! 
/*!
	
*/
class Reactor: public frame::ReactorBase{
public:
	typedef ObjectPointerT		JobT;
	typedef Object				ObjectT;
	
	Reactor(SchedulerBase &_rsched);
	~Reactor();
	bool init(size_t _cp);
	//signal a specific object
	void raise(size_t _pos);
	void run();
	size_t capacity()const;
	size_t size() const;
	bool empty()const;
	bool full()const;
	
	bool push(JobT &_rcon);
	void prepare();
	void unprepare();
private:
	
	/*virtual*/ bool raise(UidT const& _robjuid, Event const& _re);
	/*virtual*/ void stop();
	/*virtual*/ void update();
	
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

