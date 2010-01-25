#include "system/debug.hpp"
#include "system/common.hpp"
#include "utility/binaryseeker.hpp"

class MemoryFile{
public:
	MemoryFile(int64 _cp = -1L);
	~MemoryFile();
	int read(char *_pb, uint32 _bl, int64 _off);
	int write(const char *_pb, uint32 _bl, int64 _off);
	int read(char *_pb, uint32 _bl);
	int write(const char *_pb, uint32 _bl);
	int64 seek(int64 _pos, SeekRef _ref = SeekBeg);
	int truncate(int64 _len);
private:
	int doFindBuffer(uint32 _idx)const;
private:
	struct Buffer{
		Buffer(uint32 _idx = 0, char *_data = NULL):idx(_idx), data(_data){}
		uint32	idx;
		char	*data;
	};
	
	
	
	typedef std::deque<Buffer>	BufferVectorTp;
	const uint64	cp;
	uint64			sz;
	uint64			off;
	BufferVectorTp	bv;
};

int MemoryFile::doFindBuffer(uint32 _idx)const{
	struct BuffCmp{
		int operator()(const  uint32 &_k1, const Buffer &_k2)const{
			if(_k1 < _k2.idx) return -1;
			if(_k2.idx < _k1) return 1;
			return 0;
		}
		int operator()(const  Buffer &_k1, const uint32 &_k2)const{
			if(_k1.idx < _k2) return -1;
			if(_k2 < _k1.idx) return 1;
			return 0;
		}
	};
	static BinarySeeker<Buffcmp>	bs;
	return bs(bv.begin(), bv.end(), _idx);
}

char *MemoryFile::doGetBuffer(uint32 _idx, uint32 &_previdx)const{

}


MemoryFile::MemoryFile(int64 _cp):cp(_cp){}

MemoryFile::~MemoryFile(){
}
int64 MemoryFile::size()const{
}
int MemoryFile::read(char *_pb, uint32 _bl, int64 _off){
	uint32		buffidx(_off / 4096);
	uint32		buffoff(_off % 4096);
	int 		rd(0);
	uint32		previdx(0xffffffff);
	if(_off >= sz) return -1;
	if((_off + _bl) > sz){
		_bl = sz - _off;
	}
	while(_bl){
		char *bf(doGetBuffer(buffidx, previdx));
		uint32 tocopy(4096 - buffoff);
		if(tocopy > _bl) tocopy = _bl;
		if(!bf){
			
			memset(_pb, '\0', tocopy);
		}else{
			memcpy(_pb, bf + buffoff, tocopy);
		}
		buffoff = 0;
		++buffidx;
		_pb += tocopy;
		rd += tocopy;
		_bl -= tocopy;
		buffoff = 0;
		if(_bl) ++bufidx;
	}
	if(_bl && rd == 0) return -1;
	return rd;
}

int MemoryFile::write(const char *_pb, uint32 _bl, int64 _off){
	uint32		buffidx(_off / 4096);
	uint32		buffoff(_off % 4096);
	int			wd(0);
	uint32		previdx(0xffffffff);
	while(_bl){
		bool 	created(false);
		char* 	bf(doCreateBuffer(buffidx, previdx, created));
		if(!bf) break;
		uint32 tocopy(4096 - buffoff);
		if(tocopy > _bl) tocopy = _bl;
		if(created){
			memset(bf, '\0', buffoff); 
			memset(bf + buffoff + tocopy, '\0', 4096 - buffoff - tocopy);
		}
		memcpy(bf + buffoff, _pb, tocopy);
		_pb += tocopy;
		wd += tocopy;
		_bl -= tocopy;
		buffoff = 0;
		if(_bl) ++bufidx;
	}
	if(_bl && wd == 0){
		errno = ENOSPC;
		return -1;
	}
	
	if(sz < _off + wd) sz = _off + wd;
	return wd;
}

char *MemoryFile::doCreateBuffer(uint32 _idx, uint32 &_previdx, bool &_created){
	if(bv.empty() || _idx > bv.back().idx){
		bv.push_back(Buffer(_idx));
		if((bv.size() * 4096 + 4096) > cp) return NULL;
		bv.back().data = new char[4096];
		_created = true;
		return bv.back().data;
	}
	if(bv.back().idx == _idx) return bv.back().data();
	int idx(-1);
	if(_previdx < bv.size()){
		if((_previdx + 1) < bv.size()){
			++_previdx;
			if(bv[_previdx].idx == _idx){
				return bv[_previdx].data;
			}else if(bv[_previdx].idx > _idx){
				idx = _previdx;
			}
		}else{
			idx = doFindBuffer(_idx);
			if(idx >= 0){
				//buffer found
				return bv[idx].data;
			}
			//buffer not found
			idx = -idx - 1;//the insert position
		}
	}else{
		idx = doFindBuffer(_idx);
		if(idx >= 0){
			//buffer found
			return bv[idx].data;
		}
		//buffer not found
		idx = -idx - 1;//the insert position
	}

	if((bv.size() * 4096 + 4096) > cp) return NULL;
	char * b = new char[4096];
	bv.insert(bv.begin() + idx, Buffer(_idx, b));
	_created = true;
	return b;
}

int MemoryFile::read(char *_pb, uint32 _bl){
	int rv(read(_pb, _bl, crtoff));
	if(rv > 0) crtoff += rv;
	return rv;
}

int MemoryFile::write(const char *_pb, uint32 _bl){
	int rv(write(_pb, _bl, crtoff));
	if(rv > 0) crtoff += rv;
	return rv;
}

int64 MemoryFile::seek(int64 _pos, SeekRef _ref){
	switch(_ref){
		case SeekBeg:
			if(_pos >= cp) return -1;
			return crtoff = _pos;
		case SeekCur:
			if(crtoff + _pos > cp) return -1;
			crtoff += _pos;
			return crtoff;
		case SeekEnd:
			if(sz + _pos > cp) return -1;
			crtoff = sz + pos;
			return crtoff;
	}
}

int MemoryFile::truncate(int64 _len){
	//TODO:
	return -1;
}

int main(int argc, char *argv[]){
	
}
