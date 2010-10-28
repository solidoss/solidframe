/* Declarations file parameter.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ALGORITHM_PROTOCOL_PARAMETERX_HPP
#define ALGORITHM_PROTOCOL_PARAMETERX_HPP

#include "system/common.hpp"

namespace protocol{
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

}//namespace protocol

#ifndef NINLINES
#include "algorithm/protocol/parameter.ipp"
#endif

#endif
