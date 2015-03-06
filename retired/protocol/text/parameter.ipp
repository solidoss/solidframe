// protocol/text/parameter.ipp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#endif

inline Parameter::Parameter(void *_pa, void *_pb){
	a.p = _pa;
	b.p = _pb;
}
inline Parameter::Parameter(void *_p, uint64 _u){
	a.p = _p;
	b.u64 = _u;
}
inline Parameter::Parameter(uint64 _u, void *_p){
	a.u64 = _u;
	b.p = _p;
}
inline Parameter::Parameter(uint32 _u, void *_p){
	a.u64 = _u;
	b.p = _p;
}

inline Parameter::Parameter(uint64 _ua, uint64 _ub){
	a.u64 = _ua;
	b.u64 = _ub;
}
inline Parameter::Parameter(int _i){
	a.i = _i;
	b.p = NULL;
}

inline Parameter::Parameter(const Parameter &_rp){
	a.u64 = _rp.a.u64;
	b.u64 = _rp.b.u64;
}

#ifdef NINLINES
#undef inline
#endif

