// system/error.hpp
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

#include "config.h"

#ifdef HAS_CPP11
#define ERROR_NS std
#else
#define ERROR_NS boost::system
#endif

#ifdef HAS_CPP11
#include <system_error>
#else
#include "boost/system/error_code.hpp"
#endif

#include <vector>
#include <ostream>

namespace solid{

ERROR_NS::error_code last_system_error();

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
		ERROR_NS::error_code const	&_code,
		unsigned _line = -1,
		const char *_file = NULL
	):	value(_code.value()), category(&_code.category()), line(_line), file(_file){}
	
	ERROR_NS::error_code errorCode()const{
		return ERROR_NS::error_code(value, category ? *category : ERROR_NS::system_category());
	}
		
	int								value;
	ERROR_NS::error_category const	*category;
	unsigned						line;
	const char						*file;
};

typedef std::vector<ErrorStub>	ErrorVectorT;

ERROR_NS::error_category const	&error_category_get();
ERROR_NS::error_code 			error_make(Errors _err);

void specific_error_clear();
void specific_error_push(
	int _value,
	ERROR_NS::error_category const	*_category,
	unsigned _line = -1,
	const char *_file = NULL
);

void specific_error_push(
	ERROR_NS::error_code const	&_rcode,
	unsigned _line = -1,
	const char *_file = NULL
);

ERROR_NS::error_code specific_error_back();

#define SPECIFIC_ERROR_PUSH1(c)	solid::specific_error_push((c), __LINE__, __FILE__) 
#define SPECIFIC_ERROR_PUSH2(v, c)	solid::specific_error_push(static_cast<int>((v)), &(c), __LINE__, __FILE__) 

#define SPECIFIC_ERROR_INIT1(c)	solid::specific_error_clear();solid::specific_error_push((c), __LINE__, __FILE__) 
#define SPECIFIC_ERROR_INIT2(v, c)	solid::specific_error_clear();solid::specific_error_push(static_cast<int>((v)), (c), __LINE__, __FILE__) 

ErrorVectorT const & specific_error_get();
void specific_error_print(std::ostream &_ros, const bool _withcodeinfo = true);


typedef ERROR_NS::error_condition	ErrorConditionT;
typedef ERROR_NS::error_condition	ErrorCodeT;


}//namespace solid

#endif
