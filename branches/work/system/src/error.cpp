// system/src/error.cpp
//
// Copyright (c) 2013,2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "system/error.hpp"
#include "system/thread.hpp"

namespace solid{

ERROR_NS::error_code last_system_error(){
#ifdef ON_WINDOWS
	const DWORD err = GetLastError();
	return ERROR_NS::error_code(err, ERROR_NS::system_category());
#else
	return ERROR_NS::error_code(errno, ERROR_NS::system_category());
#endif
}

ERROR_NS::error_code last_error(){
	
}
void last_error(int _err){
	
}

}//namespace solid
