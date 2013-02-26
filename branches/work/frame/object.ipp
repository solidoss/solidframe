/* Inline implementation file object.ipp
	
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



inline IndexT Object::threadId()const{
#ifdef HAS_STDATOMIC
	return thrid.load(std::memory_order_relaxed);
#else
	return thrid.load(boost::atomic::memory_order_relaxed);
#endif
}
inline void Object::threadId(const IndexT &_thrid){
#ifdef HAS_STDATOMIC
	thrid.store(_thrid, std::memory_order_relaxed);
#else
	thrid.store(_thrid, boost::atomic::memory_order_relaxed);
#endif

}

inline ulong Object::grabSignalMask(ulong _leave){
// 	ulong sm = smask;
// 	smask = sm & _leave;
// 	return sm;
#ifdef HAS_STDATOMIC
	return smask.fetch_and(_leave, std::memory_order_relaxed);
#else
	return smask.fetch_and(_leave, boost::atomic::memory_order_relaxed);
#endif
}
inline bool Object::notified() const {
#ifdef HAS_STDATOMIC
	return smask.load(std::memory_order_relaxed) != 0;
#else
	return smask.load(boost::atomic::memory_order_relaxed) != 0;
#endif
}
inline bool Object::notified(ulong _s) const{
	return (smask & _s) != 0;
#ifdef HAS_STDATOMIC
	return (smask.load(std::memory_order_relaxed) & _s) != 0;
#else
	return (smask.load(boost::atomic::memory_order_relaxed) &_s) != 0;
#endif

}

inline bool Object::notify(ulong _smask){
// 	ulong oldmask = smask;
// 	smask |= _smask;
// 	return (smask != oldmask) && signaled(S_RAISE);
#ifdef HAS_STDATOMIC
	ulong osm = smask.fetch_or(_smask, std::memory_order_relaxed);
#else
	ulong osm = smask.fetch_or(_smask, boost::atomic::memory_order_relaxed);
#endif
	return (_smask & S_RAISE) && !(osm & S_RAISE); 
}


inline void Object::id(const IndexT &_fullid){
	fullid = _fullid;
}

#ifdef NINLINES
#undef inline
#endif
