/* Declarations file synchronization.hpp
	
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

#ifndef SYNCHROPP_HPP
#define SYNCHROPP_HPP

#include "system/cassert.hpp"
#include <semaphore.h>
//#include "mutex.hpp"

struct TimeSpec;

class Semaphore{
public:
	Semaphore(int _cnt = 0);
	~Semaphore();
	void wait();
	operator int ();
	Semaphore & operator++();
	int tryWait();
private:
	sem_t sem;
};

// template <typename T>
// class CV: public Mutex{
// public:
// 	CV(const T &_v):val(_v),dv(_v){}
// 	~CV(){}
// 	typedef CV<T> ThisT;
// 	
// 	ThisT & operator=(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 		return *this;
// 	}
// 	void signal(const T &_v){
// 		val = _v;
// 		cond.signal();
// 	}
// 	void broadcast(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 	}
// 	ThisT & operator ++ (){
// 		++val;
// 		cond.signal();
// 	}
// 	ThisT & operator -- (){
// 		--val;
// 		cond.signal();
// 	}
// 	T waitFor(){
// 		cond.wait(*this);
// 		return val;
// 	}
// 	T waitFor(const T &_v){
// 		cassert(_v != dv);
// 		//this->lock();
// 		while(val!=_v){
// 			cond.wait(*this);
// 		}
// 		//this->unlock();
// 		T v = val;
// 		val = dv;
// 		return v;
// 	}
// 	T waitFor(const T &_v1, const T &_v2){
// 		cassert(_v1 != dv && _v2 != dv);
// 		while(val != _v1 && val != _v2){
// 			cond.wait(*this);
// 		}
// 		T v = val;
// 		val = dv;
// 		return v;
// 	}
// 	
// private:
// 	T				val,dv;
// 	Condition		cond;
// 	//Mutex			mut;
// };
// 
// template <typename T, T Dv>
// class FastCV: public Mutex{
// public:
// 	FastCV():val(Dv){}
// 	~FastCV(){}
// 	typedef FastCV<T,Dv> ThisT;
// 	
// 	ThisT & operator=(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 		return *this;
// 	}
// 	void signal(const T &_v){
// 		val = _v;
// 		cond.signal();
// 	}
// 	void broadcast(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 	}
// 	ThisT & operator ++ (){
// 		++val;
// 		cond.signal();
// 	}
// 	ThisT & operator -- (){
// 		--val;
// 		cond.signal();
// 	}
// 	T waitFor(){
// 		cond.wait(*this);
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	T waitFor(const T &_v){
// 		cassert(_v != Dv);
// 		//this->lock();
// 		while(val!=_v){
// 			cond.wait(*this);
// 		}
// 		//this->unlock();
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	T waitFor(const T &_v1, const T &_v2){
// 		cassert(_v1 != Dv && _v2 != Dv);
// 		while(val != _v1 && val != _v2){
// 			cond.wait(*this);
// 		}
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	
// private:
// 	T				val;
// 	Condition		cond;
// };
// 
// 
// template <typename T, T Dv>
// class MaskCV: public Mutex{
// public:
// 	MaskCV():val(Dv){}
// 	~MaskCV(){}
// 	typedef MaskCV<T,Dv> ThisT;
// 	
// 	ThisT & operator=(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 		return *this;
// 	}
// 	void signal(const T &_v){
// 		val |= _v;
// 		cond.signal();
// 	}
// 	void broadcast(const T &_v){
// 		val = _v;
// 		cond.broadcast();
// 	}
// 	ThisT & operator ++ (){
// 		++val;
// 		cond.signal();
// 	}
// 	ThisT & operator -- (){
// 		--val;
// 		cond.signal();
// 	}
// 	T waitFor(){
// 		cond.wait(*this);
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	T waitFor(const T &_v){
// 		cassert(_v != Dv);
// 		//this->lock();
// 		while(val!=_v){
// 			cond.wait(*this);
// 		}
// 		//this->unlock();
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	T waitFor(const T &_v1, const T &_v2){
// 		cassert(_v1 != Dv && _v2 != Dv);
// 		while(val != _v1 && val != _v2){
// 			cond.wait(*this);
// 		}
// 		T v = val;
// 		val = Dv;
// 		return v;
// 	}
// 	
// private:
// 	T				val;
// 	Condition		cond;
// };

#ifndef NINLINES
#include "src/synchronization.ipp"
#endif


#endif


