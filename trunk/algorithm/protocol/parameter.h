/* Declarations file parameter.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PROTOCOL_PARAMETER_H
#define PROTOCOL_PARAMETER_H

#include "system/common.h"

namespace protocol{
//!Parameter structure for reader and writer callbacks
/*!
	It keeps two objects a and b wich are unions of void* / ulong/ int.
	No type info is kept. The callbacks should know the type of data.
*/
struct Parameter{
	union{
		void 	*p;
		ulong	u;
		int		i;
	} a, b;
	//!Convenient constructor for two void pointers
	Parameter(void *_pa = NULL, void *_pb = NULL){
		a.p = _pa;
		b.p = _pb;
	}
	//!Convenient constructor for a void pointer and an ulong
	Parameter(void *_p, ulong _u){
		a.p = _p;
		b.u = _u;
	}
	//!Convenient constructor for an ulong and a void pointer
	Parameter(ulong _u, void *_p = NULL){
		a.u = _u;
		b.p = _p;
	}
	//!Convenient constructor for two ulongs
	Parameter(ulong _ua, ulong _ub){
		a.u = _ua;
		b.u = _ub;
	}
	//!Convenient constructor for one int - b will be NULL
	explicit Parameter(int _i){
		a.i = _i;
		b.p = NULL;
	}
};
}//namespace protocol


#endif
