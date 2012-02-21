/* Inline implementation file ostream.ipp
	
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
