/* Declarations file aiosocketpointer.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FOUNDATION_AIO_SOCKETPOINTER_HPP
#define FOUNDATION_AIO_SOCKETPOINTER_HPP

namespace foundation{
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
}//namespace foundation

#endif
