// utility/stream.ipp
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

inline StreamFlags::StreamFlags(uint32 _flags):flags(_flags){}

inline bool Stream::ok()const{
	return flags.flags == 0;
}
inline bool Stream::eof()const{
	return (flags.flags & (StreamFlags::IEof | StreamFlags::OEof)) != 0;
}
inline bool Stream::bad()const{
	return (flags.flags & (StreamFlags::IBad | StreamFlags::OBad)) != 0;
}
inline bool Stream::fail()const{
	return bad() | ((flags.flags & (StreamFlags::IFail | StreamFlags::OFail)) != 0);
}

#ifdef NINLINES
#undef inline
#endif


