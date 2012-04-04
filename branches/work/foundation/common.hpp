/* Declarations file common.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_COMMON_HPP
#define FOUNDATION_COMMON_HPP

#include <utility>
#include "utility/common.hpp"

namespace foundation{

//! Some return values
enum RetValEx {
	LEAVE = CONTINUE + 1,
	REGISTER, UNREGISTER, RTOUT, WTOUT, DONE
};

//! Some signals
enum Signals{
	//simple:
	S_RAISE = 1,
	S_UPDATE = 2,
	S_SIG = 4,
	S_KILL = 1<<8,
	S_IDLE = 1<<9,
	S_ERR  = 1<<10,
	S_STOP = 1<<11,
};

//! Some events
enum Events{
	OKDONE = 1, //Successfull asynchrounous completion
	ERRDONE = 2,//Unsuccessfull asynchrounous completion
	INDONE = 4,//Successfull input asynchrounous completion
	OUTDONE = 8,//Successfull output asynchrounous completion
	TIMEOUT = 16,//Unsuccessfull asynchrounous completion due to timeout
	SIGNALED = 32,
	IODONE = 64,
	RESCHEDULED = 128,
	TIMEOUT_RECV = 256,
	TIMEOUT_SEND = 512,
	
};

enum Consts{
	MAXTIMEOUT = (0xffffffffUL>>1)/1000
};
#ifdef _LP64

//64 bit architectures
#ifdef UINDEX32
//32 bit indexes
typedef uint32 IndexT;
#define ID_MASK 0xffffffff
#else
//64 bit indexes
typedef uint64 IndexT;
#define ID_MASK 0xffffffffffffffffULL
#endif

#else

//32 bit architectures
#ifdef UINDEX64
//64 bit indexes
typedef uint64 IndexT;
#define ID_MASK 0xffffffffffffffffULL
#else
//32 bit indexes
typedef uint32 IndexT;
#define ID_MASK 0xffffffffUL
#endif

#endif

#define INVALID_INDEX ID_MASK

typedef std::pair<IndexT, uint32>	UidT;
typedef UidT						ObjectUidT;
typedef UidT						SignalUidT;
typedef UidT						FileUidT;
typedef UidT						RequestUidT;


inline const UidT& invalid_uid(){
	static const UidT u(INVALID_INDEX, 0xffffffff);
	return u;
}

inline bool is_valid_uid(const UidT &_ruid){
	return _ruid.first != INVALID_INDEX;
}

inline bool is_invalid_uid(const UidT &_ruid){
	return _ruid.first == INVALID_INDEX;
}

inline bool is_valid_index(const IndexT &_idx){
	return _idx != INVALID_INDEX;
}

inline bool is_invalid_index(const IndexT &_idx){
	return _idx == INVALID_INDEX;
}

template <class V>
typename V::value_type& safe_at(V &_v, uint _pos){
	if(_pos < _v.size()){
		return _v[_pos];
	}else{
		_v.resize(_pos + 1);
		return _v[_pos];
	}
}

template <class V>
IndexT smart_resize(V &_rv, const IndexT &_rby){
	_rv.resize(((_rv.size() / _rby) + 1) * _rby);
	return _rv.size();
}

template <class V>
IndexT fast_smart_resize(V &_rv, const size_t _bitby){
	_rv.resize(((_rv.size() >> _bitby) + 1) << _bitby);
	return _rv.size();
}


}//namespace foundation

#endif
