// frame/aio/aiosocketpointer.hpp
//
// Copyright (c) 2010, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SOCKETPOINTER_HPP
#define SOLID_FRAME_AIO_SOCKETPOINTER_HPP

namespace solid{
namespace frame{
namespace aio{

class Object;
class Socket;
class SocketPointer{
public:
	SocketPointer():ps(NULL){}
	~SocketPointer(){
		clear();
	}
	SocketPointer(const SocketPointer &_ps):ps(_ps.release()){}
	SocketPointer& operator=(const SocketPointer &_ps){
		clear(_ps.release());
		return *this;
	}
	operator bool ()const	{return ps != NULL;}
private:
	friend class Object;
	SocketPointer(Socket*_ps):ps(NULL){}
	void clear(Socket *_ps = NULL)const;
	Socket* release()const{
		Socket *tps(ps);
		ps = NULL;
		return tps;
	}
private:
	mutable Socket	*ps;
};

}//namespace aio
}//namespace frame
}//namespace solid

#endif
