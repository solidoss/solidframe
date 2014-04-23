// utility/memoryfile.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_MEMORY_FILE_HPP
#define UTILITY_MEMORY_FILE_HPP

#include "system/common.hpp"
#include "utility/binaryseeker.hpp"
#include <deque>

namespace solid{

//! Store in memory files
/*!
	It has the same interface like FileDevice.
	It has support for writing only at different offsets.
	It uses the given allocator for allocating/releasing data buffers.
	
*/
class MemoryFile{
public:
	struct Allocator{
		virtual ~Allocator(){}
		virtual uint  bufferSize()const = 0;
		virtual char* allocate() = 0;
		virtual void release(char *) = 0;
		virtual uint64 computeCapacity(uint64 _cp) = 0;
	};
	template <uint Sz = 4096>
	struct BasicAllocator: Allocator{
#ifdef HAS_SAFE_STATIC
		static BasicAllocator<Sz>& instance(){
			static BasicAllocator<Sz> ba;
			return ba;
		}
#else
	private:
		static BasicAllocator<Sz>& instanceStub(){
			static BasicAllocator<Sz> ba;
			return ba;
		}
		static void once_cbk(){
			instanceStub();
		}
	public:
		static BasicAllocator<Sz>& instance(){
			static boost::once_flag once = BOOST_ONCE_INIT;
			boost::call_once(&once_cbk, once);
			return instanceStub();
		}
#endif
		/*virtual*/ uint  bufferSize()const{return Sz;}
		/*virtual*/ char* allocate(){
			return new char[Sz];
		}
		/*virtual*/ void release(char *_p){
			delete []_p;
		}
		/*virtual*/ uint64 computeCapacity(uint64 _cp){
			return ((_cp / Sz) + 1) * Sz;
		}
	};

public:
	//! Constructor with the file capacity and the allocator
	MemoryFile(uint64 _cp = -1, Allocator &_ral = BasicAllocator<>::instance());
	//! Destructor
	~MemoryFile();
	//! Read data from file from offset
	int read(char *_pb, uint32 _bl, int64 _off);
	//! Write data to file at offset
	int write(const char *_pb, uint32 _bl, int64 _off);
	//! Read data from file
	int read(char *_pb, uint32 _bl);
	//! Write data to file
	int write(const char *_pb, uint32 _bl);
	//! Move the file cursor at position
	int64 seek(int64 _pos, SeekRef _ref = SeekBeg);
	//! Truncate the file
	int truncate(int64 _len = 0);
	//! Return the size of the file
	int64 size()const;
	int64 capacity()const;
private:
	BinarySeekerResultT doFindBuffer(uint32 _idx)const;
	BinarySeekerResultT doLocateBuffer(uint32 _idx)const;
	char *doGetBuffer(uint32 _idx)const;
	char *doCreateBuffer(uint32 _idx, bool &_created);
private:
	struct Buffer{
		Buffer(uint32 _idx = 0, char *_data = NULL):idx(_idx), data(_data){}
		uint32	idx;
		char	*data;
	};
	struct BuffCmp;
	friend struct BuffCmp;
	typedef std::deque<Buffer>	BufferVectorT;
	
	const uint64	cp;
	uint64			sz;
	uint64			off;
	mutable uint32	crtbuffidx;
	const uint32	bufsz;
	Allocator		&ra;
	BufferVectorT	bv;
};


}//namespace solid

#endif
