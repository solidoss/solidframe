// system/tuple.hpp
//
// Copyright (c) 2011 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_TUPLE_HPP
#define SYSTEM_TUPLE_HPP

#include "system/common.hpp"
#include <ostream>

namespace solid{

template <
	class T0, class T1 = NullType, class T2 = NullType, class T3 = NullType, class T4 = NullType,
	class T5 = NullType, class T6 = NullType, class T7 = NullType, class T8 = NullType, class T9 = NullType
>
struct Tuple;

//------	1 Tuple ----------------------------------------

template <class T0>
struct Tuple<
	T0, NullType, NullType, NullType, NullType,
	NullType, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(const T0&_ro0):o0(_ro0){}
	T0	o0;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<']';
		return _ros;
	}
};

template <class T0>
Tuple<T0> make_tuple(const T0& _ro0){
	return Tuple<T0>(_ro0);
}

//------	2 Tuple ----------------------------------------

template <class T0, class T1>
struct Tuple<
	T0, T1, NullType, NullType, NullType,
	NullType, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(const T0&_ro0, const T1&_ro1):o0(_ro0), o1(_ro1){}
	T0	o0;
	T1	o1;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<']';
		return _ros;
	}
};

template <class T0, class T1>
Tuple<T0, T1> make_tuple(const T0& _ro0, const T1& _ro1){
	return Tuple<T0, T1>(_ro0, _ro1);
}

//------	3 Tuple ----------------------------------------

template <class T0, class T1, class T2>
struct Tuple<
	T0, T1, T2, NullType, NullType,
	NullType, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(const T0&_ro0, const T1&_ro1, const T2&_ro2):o0(_ro0), o1(_ro1), o2(_ro2){}
	T0	o0;
	T1	o1;
	T2	o2;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<']';
		return _ros;
	}
};

template <class T0, class T1, class T2>
Tuple<T0, T1, T2> make_tuple(const T0& _ro0, const T1& _ro1, const T2& _ro2){
	return Tuple<T0, T1, T2>(_ro0, _ro1, _ro2);
}


//------	4 Tuple ----------------------------------------

template <class T0, class T1, class T2, class T3>
struct Tuple<
	T0, T1, T2, T3, NullType,
	NullType, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<']';
		return _ros;
	}
};

template <class T0, class T1, class T2, class T3>
Tuple<T0, T1, T2, T3>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3
){
	return Tuple<T0, T1, T2, T3>(_ro0, _ro1, _ro2, _ro3);
}

//------	5 Tuple ----------------------------------------

template <class T0, class T1, class T2, class T3, class T4>
struct Tuple<
	T0, T1, T2, T3, T4,
	NullType, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<']';
		return _ros;
	}
};

template <class T0, class T1, class T2, class T3, class T4>
Tuple<T0, T1, T2, T3, T4>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4
){
	return Tuple<T0, T1, T2, T3, T4>(_ro0, _ro1, _ro2, _ro3, _ro4);
}

//------	6 Tuple ----------------------------------------

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5
>
struct Tuple<
	T0, T1, T2, T3, T4,
	T5, NullType, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4,
		const T5&_ro5
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4),
		o5(_ro5){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	T5	o5;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<','<<o5<<']';
		return _ros;
	}
};

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5
>
Tuple<T0, T1, T2, T3, T4, T5>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4,
	const T5& _ro5
){
	return Tuple<T0, T1, T2, T3, T4, T5>(_ro0, _ro1, _ro2, _ro3, _ro4, _ro5);
}

//------	7 Tuple ----------------------------------------

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6
>
struct Tuple<
	T0, T1, T2, T3, T4,
	T5, T6, NullType, NullType, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4,
		const T5&_ro5, const T6&_ro6
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4),
		o5(_ro5), o6(_ro6){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	T5	o5;
	T6	o6;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<','<<o5<<','<<o6<<']';
		return _ros;
	}
};

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6
>
Tuple<T0, T1, T2, T3, T4, T5, T6>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4,
	const T5& _ro5, const T6& _ro6
){
	return Tuple<T0, T1, T2, T3, T4, T5, T6>(_ro0, _ro1, _ro2, _ro3, _ro4, _ro5, _ro6);
}

//------	8 Tuple ----------------------------------------

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7
>
struct Tuple<
	T0, T1, T2, T3, T4,
	T5, T6, T7, NullType, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4,
		const T5&_ro5, const T6&_ro6, const T7&_ro7
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4),
		o5(_ro5), o6(_ro6), o7(_ro7){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	T5	o5;
	T6	o6;
	T7	o7;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<','<<o5<<','<<o6<<','<<o7<<']';
		return _ros;
	}
};

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7
>
Tuple<T0, T1, T2, T3, T4, T5, T6, T7>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4,
	const T5& _ro5, const T6& _ro6, const T7& _ro7
){
	return Tuple<T0, T1, T2, T3, T4, T5, T6, T7>(_ro0, _ro1, _ro2, _ro3, _ro4, _ro5, _ro6, _ro7);
}

//------	9 Tuple ----------------------------------------

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7, class T8
>
struct Tuple<
	T0, T1, T2, T3, T4,
	T5, T6, T7, T8, NullType
>{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4,
		const T5&_ro5, const T6&_ro6, const T7&_ro7, const T8&_ro8
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4),
		o5(_ro5), o6(_ro6), o7(_ro7), o8(_ro8){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	T5	o5;
	T6	o6;
	T7	o7;
	T8	o8;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<','<<o5<<','<<o6<<','<<o7<<','<<o8<<']';
		return _ros;
	}
};

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7, class T8
>
Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4,
	const T5& _ro5, const T6& _ro6, const T7& _ro7, const T8& _ro8
){
	return Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8>(_ro0, _ro1, _ro2, _ro3, _ro4, _ro5, _ro6, _ro7, _ro8);
}

//------	10 Tuple ----------------------------------------

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7, class T8, class T9
>
struct Tuple{
	Tuple(){}
	Tuple(
		const T0&_ro0, const T1&_ro1, const T2&_ro2, const T3&_ro3, const T4&_ro4,
		const T5&_ro5, const T6&_ro6, const T7&_ro7, const T8&_ro8, const T9&_ro9
	):	o0(_ro0), o1(_ro1), o2(_ro2), o3(_ro3), o4(_ro4),
		o5(_ro5), o6(_ro6), o7(_ro7), o8(_ro8), o9(_ro9){}
	T0	o0;
	T1	o1;
	T2	o2;
	T3	o3;
	T4	o4;
	T5	o5;
	T6	o6;
	T7	o7;
	T8	o8;
	T9	o9;
	std::ostream& print(std::ostream &_ros)const{
		_ros<<'['<<o0<<','<<o1<<','<<o2<<','<<o3<<','<<o4<<','<<o5<<','<<o6<<','<<o7<<','<<o8<<','<<o9<<']';
		return _ros;
	}
};

template <
	class T0, class T1, class T2, class T3, class T4,
	class T5, class T6, class T7, class T8, class T9
>
Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>
make_tuple(
	const T0& _ro0, const T1& _ro1, const T2& _ro2, const T3& _ro3, const T4& _ro4,
	const T5& _ro5, const T6& _ro6, const T7& _ro7, const T8& _ro8, const T9& _ro9
){
	return Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>(_ro0, _ro1, _ro2, _ro3, _ro4, _ro5, _ro6, _ro7, _ro8, _ro9);
}

//----------------------------------------------------------
//----------------------------------------------------------

template <
	typename T0, typename T1, typename T2, typename T3, typename T4,
	typename T5, typename T6, typename T7, typename T8, typename T9
>
std::ostream& operator<<(
	std::ostream &_ros,
	const Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> &_rt){
	return _rt.print(_ros);
}

}//namespace solid

#endif

