// protocol/text/parameter.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_TEXT_PARAMETERX_HPP
#define SOLID_PROTOCOL_TEXT_PARAMETERX_HPP

#include "system/common.hpp"

namespace solid{
namespace protocol{
namespace text{

//!Parameter structure for reader and writer callbacks
/*!
	It keeps two objects a and b wich are unions of void* / ulong/ int.
	No type info is kept. The callbacks should know the type of data.
*/
struct Parameter{
	union{
		void 	*p;
		uint64	u64;
		uint32	u32;
		int		i;
	} a, b;
	//!Convenient constructor for two void pointers
	Parameter(void *_pa = NULL, void *_pb = NULL);
	//!Convenient constructor for a void pointer and an ulong
	Parameter(void *_p, uint64 _u);
	//!Convenient constructor for an ulong and a void pointer
	Parameter(uint64 _u, void *_p = NULL);
	//!Convenient constructor for two ulongs
	Parameter(uint64 _ua, uint64 _ub);
	Parameter(uint32 _u, void *_p = NULL);
	//!Convenient constructor for one int - b will be NULL
	explicit Parameter(int _i);
	Parameter(const Parameter &_rp);
};


#ifndef NINLINES
#include "protocol/text/parameter.ipp"
#endif

}//namespace text
}//namespace protocol
}//namespace solid

#endif
