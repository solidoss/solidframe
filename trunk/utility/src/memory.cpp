// utility/src/memory.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "utility/memory.hpp"
#include "system/thread.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"

namespace solid{

void EmptyChecker::add(){
	++v;
}
void EmptyChecker::sub(){
	--v;
}

EmptyChecker::~EmptyChecker(){
	if(v){
		idbgx(Debug::utility, "object check failed for "<<v);
	}
	cassert(v == 0);
}

}//namespace solid

