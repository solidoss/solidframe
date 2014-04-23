// utility/istream.ipp
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

inline bool InputStream::iok()const{
	return flags.flags == 0;
}
inline bool InputStream::ieof()const{
	return (flags.flags & StreamFlags::IEof) != 0;
}
inline bool InputStream::ibad()const{
	return (flags.flags & StreamFlags::IBad) != 0;
}
inline bool InputStream::ifail()const{
	return ibad() || ((flags.flags & StreamFlags::IFail) != 0);
}

inline InputStreamIterator::InputStreamIterator(InputStream *_ps, int64 _off):ps(_ps),off(_off){
}

inline void InputStreamIterator::reinit(InputStream *_ps, int64 _off){
	ps = _ps;
	off = _off;
}
inline int64 InputStreamIterator::start(){
	return ps->seek(off);
}

#ifdef NINLINES
#undef inline
#endif

