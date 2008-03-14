/* Inline implementation file istream.ipp
	
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

#ifndef UINLINES
#define inline
#endif

inline bool IStream::iok()const{
	return flags.flags == 0;
}
inline bool IStream::ieof()const{
	return flags.flags & StreamFlags::IEof;
}
inline bool IStream::ibad()const{
	return flags.flags & StreamFlags::IBad;
}
inline bool IStream::ifail()const{
	return ibad() | flags.flags & StreamFlags::IFail;
}

inline IStreamIterator::IStreamIterator(IStream *_ps, int64 _off):ps(_ps),off(_off){
}

inline void IStreamIterator::reinit(IStream *_ps, int64 _off){
	ps = _ps;
	off = _off;
}
inline int64 IStreamIterator::start(){
	return ps->seek(off);
}

#ifndef UINLINES
#undef inline
#endif

