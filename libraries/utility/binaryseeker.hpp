// utility/binaryseeker.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_BINARY_SEEKER_HPP
#define UTILITY_BINARY_SEEKER_HPP

#include <memory>

namespace solid{

//! A basic comparator for binary seeker using less (<) operator.
/*!
	As one can see, the keys may have different types.
	This is used for example in searching into vectors containing
	structures having the key as member. E.g.:<br>
	<code>
	struct ComplexData{<br>
		bool operator<(const ComplexData &_cd)const{<br>
			return id \< _cd.id;<br>
		}<br>
		
		bool operator<(uint32 _id)const{<br>
			return id \< _id;<br>
		}<br>
		
		uint32	id;<br>
		string	sdata;<br>
		uint64	udata;<br>
	};<br>
	
	typedef std::vector\<ComplexData> CDVectorT;
	
	BinarySeeker<> bs;
	
	void find(const CDVectorT &_rv, uint32 _d){<br>
		int rv = bs(_rv.begin(), _rv.end(), _d);<br>
	}
	</code>
*/
struct BasicComparator{
	template <typename K1, typename K2>
	int operator()(const K1 &_k1, const K2 &_k2)const{
		if(_k1 < _k2) return -1;
		if(_k2 < _k1) return 1;
		return 0;
	}
};


//! A template binary seeker using iterators
/*!
	It is designed to work both with vector type iterators
	and with pointers. E.g.:<br>
	<code>
	struct ComplexData{<br>
		uint32	id;<br>
		string	sdata;<br>
		uint64	udata;<br>
	};<br>
	
	struct ComplexDataComparator{<br>
		bool operator()(const ComplexData &_cd1, const ComplexData &_cd2)const{<br>
			return _cd1.id \< _cd2.id;<br>
		}<br>
		bool operator()(const ComplexData &_cd1, uint32 _id)const{<br>
			return _cd1.id \< _id;<br>
		}<br>
	};
	
	typedef std::vector\<ComplexData> CDVectorT;<br>
	
	BinarySeeker\<ComplexDataComparator\> bs;<br>
	
	void find(const CDVectorT &_rv, uint32 _d){<br>
		int rv = bs(_rv.begin(), _rv.end(), _d);<br>
	}
	</code>
*/

typedef std::pair<bool, size_t>	BinarySeekerResultT;

template <class Cmp = BasicComparator>
struct BinarySeeker{
	
	typedef BinarySeekerResultT ResultT;
	
	//! Seeks for requested key, within the range given by _from iterator and _to itereator
	/*!
		\retval >= 0 the position where the item was found
		\retval <  0 the (- position - 1), where the item may be inserted
		E.g. if rv is the returned value:<br>
		<code>
		if(rv<0){<br>
			int insertpos = - rv - 1;<br>
		}
		</code>
		
	*/
	template<class It, class Key>
	ResultT operator()(It _from, It _to, const Key &_rk)const{
		const It	beg(_from);
		size_t		midpos;
		while(_to > _from){
			midpos = (_to - _from) >> 1;
			int r = cmp(*(_from + midpos), _rk);
			if(!r) return ResultT(true, _from - beg + midpos);
			if(r < 0){
				_from += (midpos + 1);
			}else{
				_to = _from + midpos;
			}
		}
		return ResultT(false, _from - beg);
	}
	
	template<class It, class Key>
	ResultT first(It _from, It _to, const Key &_rk)const{
		ResultT p = (*this)(_from, _to, _rk);
		if(!p.first) return p;//not found
		
		while(p.second && !cmp(*(_from + p.second - 1), _rk)){
			p =  (*this)(_from, _from + p.second, _rk);
		}
		return p;
	}
	
	template<class It, class Key>
	ResultT last(It _from, It _to, const Key &_rk)const{
		ResultT p = (*this)(_from, _to, _rk);
		if(!p.first) return p;//not found
		while(p.second != (_to - _from - 1) && !cmp(*(_from + p.second + 1), _rk)){
			p =  (*this)(_from + p.second + 1, _to, _rk);
		}
		return p;
	}
private:
	Cmp		cmp;
};

}//namespace solid

#endif
