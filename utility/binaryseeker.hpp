/* Declarations file binaryseeker.hpp
	
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
#ifndef BINAR_SEEKER_HPP
#define BINAR_SEEKER_HPP

struct BasicComparator{
	template <typename K1, typename K2>
	int operator()(const K1 &_k1, const K2 &_k2)const{
		if(_k1 < _k2) return -1;
		if(_k2 < _k1) return 1;
		return 0;
	}
};

template <class Cmp = BasicComparator>
struct BinarySeeker{
	template<class It, class Key>
	int operator()(It _from, It _to, const Key &_rk){
		const It beg(_from);
		register int midpos;
		while(_to > _from){
			midpos = (_to - _from) >> 1;
			int r = cmp(*(_from + midpos), _rk);
			if(!r) return _from - beg + midpos;
			if(r < 0){
				_from += (midpos + 1);
			}else{
				_to = _from + midpos;
			}
		}
		return (- (_from - beg) - 1);
	}
	
	template<class It, class Key>
	int first(It _from, It _to, const Key &_rk){
		int p = (*this)(_from, _to, _rk);
		if(p < 0) return p;//not found
		while(p && !cmp(_from + p - 1, _rk)){
			p =  (*this)(_from, _from + p, _rk);
		}
		return p;
	}
	
	template<class It, class Key>
	int last(It _from, It _to, const Key &_rk){
		int p = (*this)(_from, _to, _rk);
		if(p < 0) return p;//not found
		while(p != (_to - _from - 1) && !cmp(_from + p + 1, _rk)){
			p =  (*this)(_from + p + 1, _to, _rk);
		}
		return p;
	}
private:
	Cmp		cmp;
};

#endif
