/* Inline implementation file parameter.ipp
	
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

#ifndef UINLINES
#define inline
#endif

namespace protocol{

inline Parameter::Parameter(void *_pa, void *_pb){
	a.p = _pa;
	b.p = _pb;
}
inline Parameter::Parameter(void *_p, ulong _u){
	a.p = _p;
	b.u = _u;
}
inline Parameter::Parameter(ulong _u, void *_p){
	a.u = _u;
	b.p = _p;
}
inline Parameter::Parameter(ulong _ua, ulong _ub){
	a.u = _ua;
	b.u = _ub;
}
inline Parameter::Parameter(int _i){
	a.i = _i;
	b.p = NULL;
}

}//namespace protocol

#ifndef UINLINES
#undef inline
#endif

