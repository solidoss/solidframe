// utility/ostream.ipp
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


inline bool OutputStream::ook()const{
	return flags.flags == 0;
}
inline bool OutputStream::oeof()const{
	return (flags.flags & StreamFlags::OEof) != 0;
}
inline bool OutputStream::obad()const{
	return (flags.flags & StreamFlags::OBad) != 0;
}
inline bool OutputStream::ofail()const{
	return obad() || ((flags.flags & StreamFlags::OFail) != 0);
}

inline OutputStreamIterator::OutputStreamIterator(OutputStream *_ps, int64 _off):ps(_ps),off(_off){
}
inline void OutputStreamIterator::reinit(OutputStream *_ps, int64 _off){
	ps = _ps;
	off = _off;
}
inline int64 OutputStreamIterator::start(){
	return ps->seek(off);
}


#ifdef NINLINES
#undef inline
#endif
