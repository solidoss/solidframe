// solid/system/src/error.cpp
//
// Copyright (c) 2013,2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/system/error.hpp"

namespace solid{

ErrorCodeT last_system_error(){
#ifdef SOLID_ON_WINDOWS
	const DWORD err = GetLastError();
	return ErrorCodeT(err, ERROR_NS::system_category());
#else
	return ErrorCodeT(errno, ERROR_NS::system_category());
#endif
}

ERROR_NS::error_category const	&error_category_get(){
	//TODO: implement an error_category
	return ERROR_NS::generic_category();
}

ErrorCodeT error_make(Errors _err){
	return ErrorCodeT(static_cast<int>(_err), error_category_get());
}


}//namespace solid
