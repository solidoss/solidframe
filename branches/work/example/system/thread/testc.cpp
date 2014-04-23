// testc.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/timespec.hpp"
using namespace std;


int main(){
	Mutex 		m;
	Condition	c;
	TimeSpec ts;
	m.lock();
	cout<<"before enter"<<endl;
	ts.currentRealTime();
	cout<<"tv_sec = "<<ts.tv_sec<<" ts.tv_nsec = "<<ts.tv_nsec<<endl;
	cout<<"time(null) = "<<time(NULL)<<endl;
	ts.tv_sec += 5;
	ts.tv_nsec += 100;
	c.wait(m, ts);
	cout<<"after exit"<<endl;
	m.unlock();
	return 0;
}

