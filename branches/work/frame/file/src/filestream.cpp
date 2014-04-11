// frame/file/src/filestore.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "system/debug.hpp"
#include "frame/file/filestream.hpp"

using namespace std;

namespace solid{
namespace frame{
namespace file{

FileBuf::FileBuf(
	FilePointerT &_rptr,
	char* _buf, size_t _bufcp
):dev(_rptr), buf(_buf), bufcp(_bufcp), off(0){
	if(_bufcp){
		setp(NULL, NULL);
		resetGet();
	}
}
FileBuf::FileBuf(
	char* _buf, size_t _bufcp
): buf(_buf), bufcp(_bufcp), off(0){
	if(_bufcp){
		setp(NULL, NULL);
		resetGet();
	}
}
FileBuf::~FileBuf(){
	idbgx(dbgid(), "");
	if(dev.get()){
		sync();
		dev.clear();
	}
}

FilePointerT& FileBuf::device(){
	idbgx(dbgid(), "");
	return dev;
}
void FileBuf::device(FilePointerT &_rptr){
	idbgx(dbgid(), "");
	dev = _rptr;
}

/*virtual*/ streamsize FileBuf::showmanyc(){
	idbgx(dbgid(), "");
	return 0;
}

/*virtual*/ FileBuf::int_type FileBuf::underflow(){
	//idbgx(dbgid(), "");
	if(hasBuf()){
		if(hasPut()){
			cassert(false);
			if(!flushPut()){
				return 0;
			}
		}
		if(gptr() < egptr()){ // buffer not exhausted
			return traits_type::to_int_type(*gptr());
		}
		//refill rbuf
		idbgx(dbgid(), "read "<<bufcp<<" from "<<off);
		int 	rv = dev->read(buf, bufcp, off);
		if(rv > 0){
			char	*end = buf + rv;
			setg(buf, buf, end);
			//rbufsz = rv;
			return traits_type::to_int_type(*gptr());
		}
	}else{
		//very inneficient
		char c;
		int rv = dev->read(&c, 1, off);
		if(rv == 1){
			return traits_type::to_int_type(c);
		}
	}
	//rbufsz = 0;
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::uflow(){
	//idbgx(dbgid(), "");
	if(hasBuf()){
		return streambuf::uflow();
	}else{
		//very inneficient
		char		c;
		const int	rv = dev->read(&c, 1, off);
		if(rv == 1){
			++off;
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::pbackfail(int_type _c){
	idbgx(dbgid(), ""<<_c);
	return traits_type::eof();
}

int FileBuf::writeAll(const char *_s, size_t _n){
	int wcnt = 0;
	int	rv;
	do{
		rv = dev->write(_s, _n, off);
		if(rv > 0){
			_s += rv;
			_n -= rv;
			wcnt += rv;
		}else{
			wcnt = 0;
		}
	}while(rv > 0 && _n != 0);
	return wcnt;
}

/*virtual*/ FileBuf::int_type FileBuf::overflow(int_type _c){
	idbgx(dbgid(), ""<<_c<<" off = "<<off);
	if(hasBuf()){
		if(pptr() == NULL){
			if(hasGet()){
				off += (gptr() - buf);
				resetGet();
			}
			resetPut();
			if(_c != traits_type::eof()){
				*pptr() = _c;
				pbump(1);
			}
			return traits_type::to_int_type(_c);
		}
		
		char *endp = pptr();
		if(_c != traits_type::eof()){
			*endp = _c;
			++endp;
		}
		
		size_t towrite = endp - pbase();
		int rv = writeAll(pbase(), towrite);
		if(rv == towrite){
			off += towrite;
			resetPut();
		}else{
			setp(NULL, NULL);
		}
	}else{
		const char	c = _c;
		const int	rv = dev->write(&c, 1, off);
		if(rv == 1){
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::pos_type FileBuf::seekoff(
	off_type _off, ios_base::seekdir _way,
	ios_base::openmode _mode
){
	idbgx(dbgid(), "seekoff = "<<_off<<" way = "<<_way<<" mode = "<<_mode<<" off = "<<off);
	if(hasBuf()){
		if(hasPut()){
			cassert(!hasGet());
			if(!flushPut()){
				return pos_type(-1);
			}
			setp(NULL, NULL);
			resetGet();
		}
		int64 newoff = 0;
		if(_way == ios_base::beg){
		}else if(_way == ios_base::end){
			newoff = off + (pptr() - pbase());
			int64 devsz = dev->size();
			if(newoff <= devsz){
				newoff = devsz;
			}
		}else if(!_off){//cur
			return off + (gptr() - eback());
		}else{
			newoff = off;
			newoff += (gptr() - eback());
		}
		newoff += _off;
		int		bufoff = egptr() - eback();
		if(newoff >= off &&  newoff < (off + bufoff)){
			int newbufoff = newoff - off;
			setg(buf, buf + newbufoff, egptr());
			newoff = off + newbufoff;
		}else{
			off = newoff;
			setp(NULL, NULL);
			resetGet();
		}
		return newoff;
	}else{
		if(_way == ios_base::beg){
			off = _off;
		}else if(_way == ios_base::end){
			off = dev->size() + _off;
		}else{//cur
			off += _off;
		}
		return off;
	}
}

/*virtual*/ FileBuf::pos_type FileBuf::seekpos(
	pos_type _pos,
	ios_base::openmode _mode
){
	idbgx(dbgid(), ""<<_pos);
	return seekoff(_pos, std::ios_base::beg, _mode);
}

/*virtual*/ int FileBuf::sync(){
	idbgx(dbgid(), "");
	if(hasPut()){
		flushPut();
	}
	dev->flush();
	return 0;
}

// /*virtual*/ void FileBuf::imbue(const locale& _loc){
// 	
// }

/*virtual*/ streamsize FileBuf::xsgetn(char_type* _s, streamsize _n){
	idbgx(dbgid(), ""<<_n<<" off = "<<off);
	if(hasBuf()){
		if(hasPut()){
			if(!flushPut()){
				return 0;
			}
			setp(NULL, NULL);
			resetGet();
		}
		
		if (gptr() < egptr()){ // buffer not exhausted
			streamsize sz = egptr() - gptr();
			if(sz > _n){
				sz = _n;
			}
			memcpy(_s, gptr(), sz);
			gbump(sz);
			return sz;
		}
		//read directly in the given buffer
		int 	rv = dev->read(_s, _n, off);
		if(rv > 0){
			off += rv;
			resetGet();
			return rv;
		}
	}else{
		const int	rv = dev->read(_s, _n, off);
		if(rv > 0){
			off += rv;
			return rv;
		}
	}
	//rbufsz = 0;
	return 0;
}

bool FileBuf::flushPut(){
	if(pptr() != epptr()){
		size_t towrite = pptr() - pbase();
		idbgx(dbgid(), ""<<towrite<<" off = "<<off);
		int rv = writeAll(buf, towrite);
		if(rv == towrite){
			off += towrite;
			resetBoth();
		}else{
			setp(NULL, NULL);
			return false;
		}
	}
	return true;
}

/*virtual*/ streamsize FileBuf::xsputn(const char_type* _s, streamsize _n){
	idbgx(dbgid(), ""<<_n<<" off = "<<off);
	if(hasBuf()){
		//NOTE: it should work with the following line too
		//return streambuf::xsputn(_s, _n);
		
		if(pptr() == NULL){
			if(hasGet()){
				off += (gptr() - buf);
				resetGet();
			}
			resetPut();
		}
		
		const streamsize	sz = _n;
		const size_t		wleftsz = bufcp - (pptr() - pbase());
		size_t				towrite = _n;
		
		if(wleftsz < towrite){
			towrite = wleftsz;
		}
		memcpy(pptr(), _s, towrite);
		_n -= towrite;
		_s += towrite;
		pbump(towrite);
		
		if(_n != 0 || towrite == wleftsz){
			if(!flushPut()){
				return 0;
			}
			if(_n && _n <= bufcp/2){
				memcpy(pptr(), _s, _n);
				pbump(_n);
			}else if(_n){
				size_t towrite = _n;
				int rv = writeAll(_s, towrite);
				if(rv == towrite){
					off += rv;
					resetPut();
				}else{
					return 0;
				}
			}else{
				resetPut();
			}
		}
		return sz;
	}else{
		const int	rv = dev->write(_s, _n, off);
		if(rv > 0){
			off += rv;
			return rv;
		}
	}
	return 0;
}

}//namespace file
}//namespace frame
}//namespace solid
