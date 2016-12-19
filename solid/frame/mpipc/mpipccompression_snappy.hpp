// solid/frame/mpipc/mpipccompression_snappy.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPCCOMPRESSION_SNAPPY_HPP
#define SOLID_FRAME_MPIPC_MPIPCCOMPRESSION_SNAPPY_HPP

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol.hpp"
#include "snappy.h"

namespace solid{
namespace frame{
namespace mpipc{
namespace snappy{

struct Engine{
	const size_t	buff_threshold;
	const size_t	diff_threshold;
	
	Engine(size_t _threshold): threshold(_threshold){}
	
	//compression:
	size_t operator()(char* _piobuf, size_t _bufsz, ErrorConditionT &){
		
		if(_bufsz > threshold){
		
			char	tmpbuf[Protocol::MaxPacketDataSize];
			
			size_t len = 0;
			snappy::RawCompress(_piobuf, _bufsz, _pto, &len);
			if((_bufsz - len) < threshold){
				
			}
			return len;
		}else{
			return 0;
		}
	}
	
	//decompression:
	size_t operator()(char* _pto, const char* _pfrom, size_t _from_sz, ErrorConditionT &_rerror){
		size_t  newlen = 0;
		if(snappy::GetUncompressedLength(_pb, _bl, &newlen)){    
		}else{
			_rerror = error_compression_engine;
			return 0;
		}
		
		if(snappy::RawUncompress(_pfrom, _frsz, _pto)){
		}else{
			_rerror = error_compression_engine;
			return 0;
		}
		return newlen;
	}
};

inline void setup(mpipc::Configuration &_rcfg, size_t _buff_threshold = 1024, size_t _diff_threshold = 1024){
	_rcfg.reader.decompress_fnc = Engine(_threshold);
	_rcfg.writer.inplace_compress_fnc = Engine(_threshold);
	
}

}//namespace snappy
}//namespace mpipc
}//namespace frame
}//namespace solid


#endif
