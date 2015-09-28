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

void specific_error_clear(){
	Thread::current().specificErrorClear();
}

void specific_error_push(
	int _value,
	ERROR_NS::error_category const	*_category,
	unsigned _line,
	const char *_file
){
	Thread::current().specificErrorPush(ErrorStub(_value, _category, _line, _file));
}

void specific_error_push(
	ErrorCodeT const	&_code,
	unsigned _line,
	const char *_file
){
	Thread::current().specificErrorPush(ErrorStub(_code, _line, _file));
}


ErrorVectorT const & specific_error_get(){
	return Thread::current().specificErrorGet();
}
ErrorCodeT specific_error_back(){
	ErrorVectorT const & errvec = Thread::current().specificErrorGet();
	if(errvec.empty()){
		return ErrorCodeT(0, ERROR_NS::generic_category());
	}else{
		return errvec.back().errorCode();
	}
}
void specific_error_print(std::ostream &_ros, const bool _withcodeinfo){
	const ErrorVectorT &rerrvec = Thread::current().specificErrorGet();
	for(ErrorVectorT::const_reverse_iterator it(rerrvec.rbegin()); it != rerrvec.rend(); ++it){
		ErrorCodeT err = it->errorCode();
		_ros<<err.category().name();
		if(_withcodeinfo){
			_ros<<'('<<it->file<<':'<<it->line<<')';
		}
		_ros<<':'<<' '<<err.message()<<';'<<' ';
	}
}

}//namespace solid
