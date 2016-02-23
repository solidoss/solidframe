// serialization/src/binarybasic.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "serialization/binarybasic.hpp"
#include "system/cassert.hpp"

namespace solid{
namespace serialization{
namespace binary{
//========================================================================
char* crossStore(char *_pd, uint8 _v){
	uint8			*pd = reinterpret_cast<uint8*>(_pd);
	const size_t	sz = max_padded_byte_cout(_v);
	const bool 		ok = compute_value_with_crc(*pd, static_cast<uint8>(sz));
	if(ok){
		++pd;
		if(sz){
			*pd = _v;
		}
		return _pd + sz + 1;
	}
	return nullptr;
}
char* crossStore(char *_pd, uint16 _v){
	uint8			*pd = reinterpret_cast<uint8*>(_pd);
	const size_t	sz = max_padded_byte_cout(_v);
	const bool 		ok = compute_value_with_crc(*pd, static_cast<uint8>(sz));
	if(ok){
		++pd;
		
		switch(sz){
			case 0:
				break;
			case 1:
				*pd = (_v & 0xff);
				break;
			case 2:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				break;
			default:
				return nullptr;
		}
		
		return _pd + sz + 1;
	}
	return nullptr;
}
char* crossStore(char *_pd, uint32 _v){
	uint8			*pd = reinterpret_cast<uint8*>(_pd);
	const size_t	sz = max_padded_byte_cout(_v);
	const bool 		ok = compute_value_with_crc(*pd, static_cast<uint8>(sz));
	if(ok){
		++pd;
		
		switch(sz){
			case 0:
				break;
			case 1:
				*pd = (_v & 0xff);
				break;
			case 2:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				break;
			case 3:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				break;
			case 4:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				break;
			default:
				return nullptr;
		}
		
		return _pd + sz + 1;
	}
	return nullptr;
}
char* crossStore(char *_pd, uint64 _v){
	uint8			*pd = reinterpret_cast<uint8*>(_pd);
	const size_t	sz = max_padded_byte_cout(_v);
	const bool 		ok = compute_value_with_crc(*pd, static_cast<uint8>(sz));
	if(ok){
		++pd;
		
		switch(sz){
			case 0:
				break;
			case 1:
				*pd = (_v & 0xff);
				break;
			case 2:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				break;
			case 3:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				break;
			case 4:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				break;
			case 5:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				_v >>= 8;
				*(pd + 4) = (_v & 0xff);
				break;
			case 6:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				_v >>= 8;
				*(pd + 4) = (_v & 0xff);
				_v >>= 8;
				*(pd + 5) = (_v & 0xff);
				break;
			case 7:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				_v >>= 8;
				*(pd + 4) = (_v & 0xff);
				_v >>= 8;
				*(pd + 5) = (_v & 0xff);
				_v >>= 8;
				*(pd + 6) = (_v & 0xff);
				break;
			case 8:
				*(pd + 0) = (_v & 0xff);
				_v >>= 8;
				*(pd + 1) = (_v & 0xff);
				_v >>= 8;
				*(pd + 2) = (_v & 0xff);
				_v >>= 8;
				*(pd + 3) = (_v & 0xff);
				_v >>= 8;
				*(pd + 4) = (_v & 0xff);
				_v >>= 8;
				*(pd + 5) = (_v & 0xff);
				_v >>= 8;
				*(pd + 6) = (_v & 0xff);
				_v >>= 8;
				*(pd + 7) = (_v & 0xff);
				break;
			default:
				return nullptr;
		}
		
		return _pd + sz + 1;
	}
	return nullptr;
}

const char* crossLoad(const char *_ps, uint8 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	uint8 			v = *ps;
	const bool		ok = check_value_with_crc(v, v);
	const size_t	sz = v;
	
	if(ok){
		++ps;
		
		switch(sz){
			case 0:
				_val = 0;
				break;
			case 1:
				_val = *ps;
				break;
			default:
				return nullptr;
		}

		return _ps + static_cast<size_t>(v) + 1;
	}
	return nullptr;
}
const char* crossLoad(const char *_ps, uint16 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	uint8 			v = *ps;
	const bool		ok = check_value_with_crc(v, v);
	const size_t	sz = v;
	
	if(ok){
		++ps;
		
		switch(sz){
			case 0:
				_val = 0;
				break;
			case 1:
				_val = *ps;
				break;
			case 2:
				_val = *ps;
				_val |= static_cast<uint16>(*(ps + 1)) << 8;
				break;
			default:
				return nullptr;
		}
		return _ps + static_cast<size_t>(v) + 1;
	}
	return nullptr;
}
const char* crossLoad(const char *_ps, uint32 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	uint8 			v = *ps;
	const bool		ok = check_value_with_crc(v, v);
	const size_t	sz = v;
	
	if(ok){
		++ps;
		
		switch(sz){
			case 0:
				_val = 0;
				break;
			case 1:
				_val = *ps;
				break;
			case 2:
				_val = *ps;
				_val |= static_cast<uint32>(*(ps + 1)) << 8;
				break;
			case 3:
				_val = *ps;
				_val |= static_cast<uint32>(*(ps + 1)) << 8;
				_val |= static_cast<uint32>(*(ps + 2)) << 16;
				break;
			case 4:
				_val = *ps;
				_val |= static_cast<uint32>(*(ps + 1)) << 8;
				_val |= static_cast<uint32>(*(ps + 2)) << 16;
				_val |= static_cast<uint32>(*(ps + 3)) << 24;
				break;
			default:
				return nullptr;
		}
		return _ps + static_cast<size_t>(v) + 1;
	}
	return nullptr;
}
const char* crossLoad(const char *_ps, uint64 &_val){
	const uint8		*ps = reinterpret_cast<const uint8*>(_ps);
	uint8 			v = *ps;
	const bool		ok = check_value_with_crc(v, v);
	const size_t	sz = v;
	
	if(ok){
		++ps;
		
		switch(sz){
			case 0:
				_val = 0;
				break;
			case 1:
				_val = *ps;
				break;
			case 2:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				break;
			case 3:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				break;
			case 4:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				_val |= static_cast<uint64>(*(ps + 3)) << 24;
				break;
			case 5:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				_val |= static_cast<uint64>(*(ps + 3)) << 24;
				_val |= static_cast<uint64>(*(ps + 4)) << 32;
				break;
			case 6:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				_val |= static_cast<uint64>(*(ps + 3)) << 24;
				_val |= static_cast<uint64>(*(ps + 4)) << 32;
				_val |= static_cast<uint64>(*(ps + 5)) << 40;
				break;
			case 7:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				_val |= static_cast<uint64>(*(ps + 3)) << 24;
				_val |= static_cast<uint64>(*(ps + 4)) << 32;
				_val |= static_cast<uint64>(*(ps + 5)) << 40;
				_val |= static_cast<uint64>(*(ps + 6)) << 48;
				break;
			case 8:
				_val = *ps;
				_val |= static_cast<uint64>(*(ps + 1)) << 8;
				_val |= static_cast<uint64>(*(ps + 2)) << 16;
				_val |= static_cast<uint64>(*(ps + 3)) << 24;
				_val |= static_cast<uint64>(*(ps + 4)) << 32;
				_val |= static_cast<uint64>(*(ps + 5)) << 40;
				_val |= static_cast<uint64>(*(ps + 6)) << 48;
				_val |= static_cast<uint64>(*(ps + 7)) << 56;
				break;
			default:
				return nullptr;
		}
		return _ps + static_cast<size_t>(v) + 1;
	}
	return nullptr;
}
//========================================================================
}//namespace binary
}//namespace serialization
}//namespace solid