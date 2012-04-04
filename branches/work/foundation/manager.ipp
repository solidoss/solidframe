/* Inline implementation file manager.ipp
	
	Copyright 2011, 2012 Valentin Palade 
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

#ifdef USERVICEBITS
enum ObjectDefs{
	SERVICEBITCNT = USERVICEBITS,
	INDEXBITCNT	= sizeof(IndexT) * 8 - SERVICEBITCNT,
};
#endif

inline const IndexT& Manager::maxServiceCount()const{
#ifdef USERVICEBITS
	static const IndexT idx(IndexT(ID_MASK) >> INDEXBITCNT);
	return idx;
#else
	return max_srvc_cnt;
#endif
}
inline IndexT Manager::computeId(const IndexT &_srvidx, const IndexT &_objidx)const{
#ifdef USERVICEBITS
	return (_srvidx << INDEXBITCNT) | _objidx;
#else
	return (_srvidx << index_bit_cnt) | _objidx;
#endif
}
inline UidT Manager::makeObjectUid(const IndexT &_srvidx, const IndexT &_objidx, const uint32 _uid)const{
	return UidT(computeId(_srvidx, _objidx), _uid);
}
inline IndexT Manager::computeIndex(const IndexT &_fullid)const{
#ifdef USERVICEBITS
	return _fullid & (IndexT(ID_MASK) >> SERVICEBITCNT);
#else
	return _fullid & (IndexT(ID_MASK) >> service_bit_cnt);
#endif
}
inline IndexT Manager::computeServiceId(const IndexT &_fullid)const{
#ifdef USERVICEBITS
	return _fullid >> INDEXBITCNT;
#else
	return _fullid >> index_bit_cnt;
#endif
}

#ifdef NINLINES
#undef inline
#endif


