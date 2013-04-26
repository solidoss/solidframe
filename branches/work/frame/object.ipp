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
	return thrid.load(ATOMIC_NS::memory_order_relaxed);
}
inline void Object::threadId(const IndexT &_thrid){
	thrid.store(_thrid, ATOMIC_NS::memory_order_relaxed);

}

inline ulong Object::grabSignalMask(ulong _leave){
// 	ulong sm = smask;
// 	smask = sm & _leave;
// 	return sm;
	return smask.fetch_and(_leave, ATOMIC_NS::memory_order_relaxed);
}
inline bool Object::notified() const {
	return smask.load(ATOMIC_NS::memory_order_relaxed) != 0;
}
inline bool Object::notified(ulong _s) const{
	return (smask.load(ATOMIC_NS::memory_order_relaxed) & _s) != 0;
}

inline bool Object::notify(ulong _smask){
// 	ulong oldmask = smask;
// 	smask |= _smask;
// 	return (smask != oldmask) && signaled(S_RAISE);
	ulong osm = smask.fetch_or(_smask, ATOMIC_NS::memory_order_relaxed);
	return (_smask & S_RAISE) && !(osm & S_RAISE); 
}


inline void Object::id(const IndexT &_fullid){
	fullid = _fullid;
}

#ifdef NINLINES
#undef inline
#endif
