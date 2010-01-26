/* Declarations file atomic.hpp
	
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

#ifndef ATOMIC_HPP
#define ATOMIC_HPP

#include <bits/atomicity.h>

class Atomic{
	typedef _Atomic_word Word;
	Word val;
public:
	
	Atomic(int _av):val(_av){}
	
	inline void add(int _v){
		__gnu_cxx::__atomic_add(&val, _v);
	}
	inline Word exch_add(int _v){
		return __gnu_cxx::__exchange_and_add(&val, _v);
	}
	
	inline void operator +=(int _v){
		add(_v);
	}
	inline void operator -= (int _v){
		add(-_v);
	}
	inline void operator ++ (){
		add(1);
	}
	inline void operator -- (){
		add(-1);
	}
	inline Word operator ++ (int){
		return exch_add(1);
	}
	inline Word operator -- (int){
		return exch_add(-1);
	}
	inline operator int () const{
		return val;
	}
};
#endif

