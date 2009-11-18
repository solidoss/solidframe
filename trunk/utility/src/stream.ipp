/* Inline implementation file stream.ipp
	
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

#ifdef NINLINES
#define inline
#endif

inline StreamFlags::StreamFlags(uint32 _flags):flags(_flags){}

inline bool Stream::ok()const{
	return flags.flags == 0;
}
inline bool Stream::eof()const{
	return flags.flags & (StreamFlags::IEof | StreamFlags::OEof);
}
inline bool Stream::bad()const{
	return flags.flags & (StreamFlags::IBad | StreamFlags::OBad);
}
inline bool Stream::fail()const{
	return bad() | flags.flags & (StreamFlags::IFail | StreamFlags::OFail);
}

#ifdef NINLINES
#undef inline
#endif


