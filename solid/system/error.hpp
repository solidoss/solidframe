// solid/system/error.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SYSTEM_ERROR_HPP
#define SOLID_SYSTEM_ERROR_HPP

#include "solid/solid_config.hpp"

#ifdef SOLID_USE_CPP11
#define ERROR_NS std
#else
#define ERROR_NS boost::system
#endif

#ifdef SOLID_USE_CPP11
#include <system_error>
#else
#include "boost/solid/system/error_code.hpp"
#endif

#include <vector>
#include <ostream>

namespace solid{

typedef ERROR_NS::error_condition	ErrorConditionT;
typedef ERROR_NS::error_code		ErrorCodeT;
typedef ERROR_NS::error_category	ErrorCategoryT;

ErrorCodeT last_system_error();

enum Errors{
	ERROR_NOERROR = 0,
	ERROR_NOT_IMPLEMENTED,
	ERROR_SYSTEM,
	ERROR_THREAD_STARTED
};

struct ErrorStub{
	ErrorStub(
		int _value = -1,
		ERROR_NS::error_category const	*_category = NULL,
		unsigned _line = -1,
		const char *_file = NULL
	):	value(_value), category(_category), line(_line), file(_file){}
	
	ErrorStub(
		ErrorCodeT const	&_code,
		unsigned _line = -1,
		const char *_file = NULL
	):	value(_code.value()), category(&_code.category()), line(_line), file(_file){}
	
	ErrorCodeT errorCode()const{
		return ErrorCodeT(value, category ? *category : ERROR_NS::system_category());
	}
		
	int								value;
	ERROR_NS::error_category const	*category;
	unsigned						line;
	const char						*file;
};

typedef std::vector<ErrorStub>	ErrorVectorT;

ERROR_NS::error_category const	&error_category_get();
ErrorCodeT 	error_make(Errors _err);


}//namespace solid

#endif
