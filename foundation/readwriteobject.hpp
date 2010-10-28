/* Declarations file readwriteobject.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef FOUNDATION_READWRITEOBJECT_HPP
#define FOUNDATION_READWRITEOBJECT_HPP

#include "system/condition.hpp"
#include "system/mutex.hpp"

#include "foundation/common.hpp"

namespace foundation{

template <class B>
class ReadWriteObject: public B{
public:
	typedef B	BaseT;
	typedef ReadWriteService	ServiceT;
	ReadWriteObject():count(0),rpending(0),wpending(0),prc(NULL),pwc(NULL){}
	template <class P>
	ReadWriteObject(P &_rp):BaseT(_rp),count(0),rpending(0),wpending(0),prc(NULL),pwc(NULL){}
	void readLock(){
		ReadWriteService &rws = Manager::the().service(*this);
		Mutex &mut(rws.mutex(*this));
		mut.lock();
		if(wpending){
			++rpending;
			if(!prc) prc = rws.popCondition(*this);
			do{
				prc->wait(mut(*this));
			}while(wpending);
			if(--rpending) prc->signal();
			else rws.pushCondition(*this, prc);
		}
		++count;
		mut.unlock();
	}
	void readUnlock(){
		ReadWriteService &rws = Manager::the().service(*this);
		Mutex &mut(rws.mutex(*this));
		mut.lock();
		if(!(--count) && wpending) pwc->signal();
		mut.unlock();
	}
	void writeLock(){
		ReadWriteService &rws = Manager::the().service(*this);
		Mutex &mut(rws.mutex(*this));
		mut.lock();
		++wpending;
		if(count){
			if(!pwc) pwc = rws.popCondition(*this);
			do{
				pwc->wait(mut);
			}while(count);
		}
		++count;
		mut.unlock();
	}
	void writeUnlock(){
		ReadWriteService &rws = Manager::the().service(*this);
		Mutex &mut(rws.mutex(*this));
		mut.lock();
		--count;
		if(!(--wpending))pwc->signal();
		else{
			rws.pushCondition(*this, pwc);
			if(rpending) prc->signal();
		}
		mut.unlock();
	}
private:
	int16		count;
	int16		rpending;
	int16		wpending;
	int16		rmaxpending;//unused
	Condition	*prc;
	Condition	*pwc;
};

}//namespace foundation

#endif

