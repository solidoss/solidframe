/* Implementation file stream.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include "utility/stream.hpp"
#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "utility/iostream.hpp"
#include "utility/streampointer.hpp"

void StreamPointerBase::clear(Stream *_ps){
	if(_ps->release()) delete _ps;
}

Stream::~Stream(){
}

void Stream::close(){
}

int Stream::release(){return -1;}
//bool Stream::isOk()const{return true;}

int64 Stream::size()const{return -1;}

InputStream::~InputStream(){
}

OutputStream::~OutputStream(){
}

InputOutputStream::~InputOutputStream(){
}

#ifdef NINLINES
#include "utility/stream.ipp"
#include "utility/istream.ipp"
#include "utility/ostream.ipp"
#include "utility/iostream.ipp"
#endif

bool InputStream::readAll(char *_pd, uint32 _dl, uint32){
	int rv;
	char *pd = (char*)_pd;
	while(_dl && (rv = this->read(pd, _dl)) != (int)_dl){
		if(rv > 0){
			pd += rv;
			_dl -= rv;
		}else return false;
	}
	return true;
}

 int InputStream::read(uint64 _offset, char* _pbuf, uint32 _blen, uint32 _flags){
	 const int64	crtoff(seek(0, SeekCur));
	 
	 if(crtoff < 0) return -1;
	 if(_offset != seek(_offset)) return -1;
	 
	 int rv = read(_pbuf, _blen, _flags);
	 
	 seek(crtoff);
	 return rv;
 }


int OutputStream::write(uint64 _offset, const char *_pbuf, uint32 _blen, uint32 _flags){
	 const int64	crtoff(seek(0, SeekCur));
	 
	 if(crtoff < 0) return -1;
	 if(_offset != seek(_offset)) return -1;
	 
	 int rv = write(_pbuf, _blen, _flags);
	 
	 seek(crtoff);
	 return rv;
}
