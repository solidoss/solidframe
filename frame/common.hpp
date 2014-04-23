// frame/common.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_COMMON_HPP
#define SOLID_FRAME_COMMON_HPP

#include <utility>
#include "utility/common.hpp"
#include "system/convertors.hpp"

namespace solid{
namespace frame{

//! Some return values
// enum RetValEx {
// 	LEAVE = CONTINUE + 1,
// 	REGISTER, UNREGISTER, RTOUT, WTOUT, DONE
// };

//! Some signals
enum SignalE{
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
enum EventE{
	EventNone = 0,
	EventDoneSuccess = 1, //Successfull asynchrounous completion
	EventDoneError = 2,//Unsuccessfull asynchrounous completion
	EventDoneRecv = 4,//Successfull input asynchrounous completion
	EventDoneSend = 8,//Successfull output asynchrounous completion
	EventTimeout = 16,//Unsuccessfull asynchrounous completion due to timeout
	EventSignal = 32,
	EventDoneIO = EventDoneRecv | EventDoneSend,
	EventReschedule = 128,
	EventTimeoutRecv = 256,
	EventTimeoutSend = 512,
};

enum ConstE{
	MAXTIMEOUT = (0xffffffffUL>>1)/1000
};

#ifdef UINDEX32
typedef uint32 IndexT;
#elif defined(UINDEX64)
typedef uint64 IndexT;
#else
typedef size_t IndexT;
#endif

#define ID_MASK ((frame::IndexT)-1)

#define INVALID_INDEX ID_MASK

typedef std::pair<IndexT, uint32>	UidT;
typedef UidT						ObjectUidT;
typedef UidT						MessageUidT;
typedef UidT						FileUidT;
typedef UidT						RequestUidT;

enum{
	IndexBitCount = sizeof(IndexT) * 8,
};

inline const UidT& invalid_uid(){
	static const UidT u(INVALID_INDEX, 0xffffffff);
	return u;
}


inline bool is_valid_index(const IndexT &_idx){
	return _idx != INVALID_INDEX;
}

inline bool is_invalid_index(const IndexT &_idx){
	return _idx == INVALID_INDEX;
}

inline bool is_valid_uid(const UidT &_ruid){
	return is_invalid_index(_ruid.first);
}

inline bool is_invalid_uid(const UidT &_ruid){
	return is_invalid_index(_ruid.first);
}

template <typename T>
T unite_index(T _hi, const T &_lo, const int _hibitcnt);

template <typename T>
void split_index(T &_hi, T &_lo, const int _hibitcnt, const T &_v);

template <>
inline uint32 unite_index<uint32>(uint32 _hi, const uint32 &_lo, const int /*_hibitcnt*/){
	return bit_revert(_hi) | _lo;
}

template <>
inline uint64 unite_index<uint64>(uint64 _hi, const uint64 &_lo, const int /*_hibitcnt*/){
	return bit_revert(_hi) | _lo;
}

template <>
inline void split_index<uint32>(uint32 &_hi, uint32 &_lo, const int _hibitcnt, const uint32 &_v){
	const uint32 lomsk = bitsToMask(32 - _hibitcnt);//(1 << (32 - _hibitcnt)) - 1;
	_lo = _v & lomsk;
	_hi = bit_revert(_v & (~lomsk));
}

template <>
inline void split_index<uint64>(uint64 &_hi, uint64 &_lo, const int _hibitcnt, const uint64 &_v){
	const uint64 lomsk = bitsToMask(64 - _hibitcnt);//(1ULL << (64 - _hibitcnt)) - 1;
	_lo = _v & lomsk;
	_hi = bit_revert(_v & (~lomsk));
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



}//namespace frame
}//namespace solid

#endif
