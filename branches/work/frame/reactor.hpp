// frame/reactor.hpp
//
// Copyright (c) 2007, 2008, 2013,2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_REACTOR_HPP
#define SOLID_FRAME_REACTOR_HPP

#include "utility/dynamicpointer.hpp"
#include "frame/common.hpp"
#include "frame/reactorbase.hpp"


namespace solid{
namespace frame{

typedef DynamicPointer<Object> ObjectPointerT;

//! An object reactor
/*!
	
*/
class Reactor: public ReactorBase{
public:
	typedef Object			ObjectT;
	
	Reactor(SchedulerBase &);
	~Reactor();
	
	bool push(ObjectPointerT &_rjob);
	void run();
private:
	/*virtual*/ bool raise(UidT const& _robjuid, Event const& _re) = 0;
	/*virtual*/ void stop();
	/*virtual*/ void update();
	int doExecute(unsigned _i, ulong _evs, TimeSpec _crttout);
	int doWait(int _wt);
private:
	struct Data;
	Data	&d;
};

}//namespace frame
}//namespace solid

#endif
