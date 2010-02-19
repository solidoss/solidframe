#include "utility/memoryfile.hpp"
#include "utility/binaryseeker.hpp"
#include <cstring>
#include <cerrno>
#include "system/cassert.hpp"


struct MemoryFile::BuffCmp{
	int operator()(const  uint32 &_k1, const MemoryFile::Buffer &_k2)const{
		if(_k1 < _k2.idx) return -1;
		if(_k2.idx < _k1) return 1;
		return 0;
	}
	int operator()(const  MemoryFile::Buffer &_k1, const uint32 &_k2)const{
		if(_k1.idx < _k2) return -1;
		if(_k2 < _k1.idx) return 1;
		return 0;
	}
};


MemoryFile::MemoryFile(
	uint64 _cp,
	MemoryFile::Allocator &_ra
):cp(_cp == -1L ? -1L : _ra.computeCapacity(_cp)),sz(0), off(0), crtbuffidx(-1), bufsz(_ra.bufferSize()), ra(_ra){
}

MemoryFile::~MemoryFile(){
	for(BufferVectorT::const_iterator it(bv.begin()); it != bv.end(); ++it){
		ra.release(it->data);
	}
}
int64 MemoryFile::size()const{
	return sz;
}
int64 MemoryFile::capacity()const{
	return cp;
}
int MemoryFile::read(char *_pb, uint32 _bl){
	int rv(read(_pb, _bl, off));
	if(rv > 0) off += rv;
	return rv;
}

int MemoryFile::write(const char *_pb, uint32 _bl){
	int rv(write(_pb, _bl, off));
	if(rv > 0) off += rv;
	return rv;
}

int MemoryFile::read(char *_pb, uint32 _bl, int64 _off){
	uint32		buffidx(_off / bufsz);
	uint32		buffoff(_off % bufsz);
	int 		rd(0);
	if(_off >= sz) return -1;
	if((_off + _bl) > sz){
		_bl = sz - _off;
	}
	while(_bl){
		char *bf(doGetBuffer(buffidx));
		uint32 tocopy(bufsz - buffoff);
		if(tocopy > _bl) tocopy = _bl;
		if(!bf){
			
			memset(_pb, '\0', tocopy);
		}else{
			memcpy(_pb, bf + buffoff, tocopy);
		}
		buffoff = 0;
		_pb += tocopy;
		rd += tocopy;
		_bl -= tocopy;
		buffoff = 0;
		if(_bl) ++buffidx;
	}
	if(_bl && rd == 0) return -1;
	return rd;
}

int MemoryFile::write(const char *_pb, uint32 _bl, int64 _off){
	uint32		buffidx(_off / bufsz);
	uint32		buffoff(_off % bufsz);
	int			wd(0);
	while(_bl){
		bool 	created(false);
		char* 	bf(doCreateBuffer(buffidx, created));
		if(!bf) break;
		uint32 tocopy(bufsz - buffoff);
		if(tocopy > _bl) tocopy = _bl;
		if(created){
			memset(bf, '\0', buffoff); 
			memset(bf + buffoff + tocopy, '\0', bufsz - buffoff - tocopy);
		}
		memcpy(bf + buffoff, _pb, tocopy);
		_pb += tocopy;
		wd += tocopy;
		_bl -= tocopy;
		buffoff = 0;
		if(_bl) ++buffidx;
	}
	if(_bl && wd == 0){
		errno = ENOSPC;
		return -1;
	}
	
	if(sz < _off + wd) sz = _off + wd;
	return wd;
}

int64 MemoryFile::seek(int64 _pos, SeekRef _ref){
	switch(_ref){
		case SeekBeg:
			if(_pos >= cp) return -1;
			return off = _pos;
		case SeekCur:
			if(off + _pos > cp) return -1;
			off += _pos;
			return off;
		case SeekEnd:
			if(sz + _pos > cp) return -1;
			off = sz + _pos;
			return off;
	}
}

int MemoryFile::truncate(int64 _len){
	//TODO:
	cassert(_len == 0);
	sz = 0;
	off = 0;
	crtbuffidx = -1;
	for(BufferVectorT::const_iterator it(bv.begin()); it != bv.end(); ++it){
		ra.release(it->data);
	}
	bv.clear();
	return -1;
}

inline int MemoryFile::doFindBuffer(uint32 _idx)const{
	static BinarySeeker<BuffCmp>	bs;
	return bs(bv.begin(), bv.end(), _idx);
}

inline char *MemoryFile::doGetBuffer(uint32 _idx)const{
	int pos(doLocateBuffer(_idx));
	if(pos >= 0) return bv[pos].data;
	return NULL;
}


char *MemoryFile::doCreateBuffer(uint32 _idx, bool &_created){
	int pos(doLocateBuffer(_idx));
	if(pos >= 0){//found buffer, return the data
		return bv[pos].data;
	}
	//buffer not found
	//see if we did not reach the capacity
	if((bv.size() * bufsz + bufsz) > cp) return NULL;
	_created = true;
	pos = -pos - 1;
	char * b = ra.allocate();
	bv.insert(bv.begin() + pos, Buffer(_idx, b));
	return b;
}

int MemoryFile::doLocateBuffer(uint32 _idx)const{
	if(bv.empty() || _idx > bv.back().idx){//append
		crtbuffidx = bv.size();
		return -bv.size() - 1;
	}
	//see if it's arround the current buffer:
	if(crtbuffidx < bv.size()){
		if(bv[crtbuffidx].idx == _idx) return crtbuffidx;
		//see if its the next buffer:
		int nextidx(crtbuffidx + 1);
		if(nextidx < bv.size() && bv[nextidx].idx == _idx){
			crtbuffidx = nextidx;
			return nextidx;
		}
	}
	int pos = doFindBuffer(_idx);
	if(pos >= 0){
		crtbuffidx = pos;
		//buffer found
		return crtbuffidx;
	}
	//buffer not found
	crtbuffidx = -pos - 1;//the insert position
	return pos;
}

