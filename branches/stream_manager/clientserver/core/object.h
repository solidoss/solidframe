/* Declarations file object.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_OBJECT_H
#define CS_OBJECT_H

#include "common.h"
#include "cmdptr.h"
//#define SERVICEBITCNT	3
//#define SERVICEBITDPL	((sizeof(ulong) * 8) - SERVICEBITCNT)
//#define SERVICEBITMSK	(1+2+4)
//#define MASKBITCNT		3
//#define MASKBITDPL		((sizeof(ulong) * 8) - SERVICEBITCNT - MASKBITCNT)
//#define MASKBITMSK		(1+2+4)
//#define INDEXMSK		(~((SERVICEBITMSK << SERVICEBITDPL) | (MASKBITMSK << MASKBITDPL)))

class Mutex;
struct TimeSpec;
namespace clientserver{

class Visitor;
class Server;
class Service;
class ObjPtrBase;
class Command;

class Object{
public:
	typedef Command	CommandTp;
	enum Defs{
		SERVICEBITCNT = 8,
		INDEXBITCNT	= 32 - SERVICEBITCNT,
		INDEXMASK = 0xffffffff >> SERVICEBITCNT
	};
	
	static ulong computeIndex(ulong _fullid);
	static ulong computeServiceId(ulong _fullid);
	
	Object(ulong _fullid = 0);
	
	//getters:
	uint serviceid()	const 	{return computeServiceId(fullid);}
	ulong id()			const 	{return fullid;}
	ulong index()		const	{return computeIndex(fullid);}
	
	void getThread(uint &_rthid, uint &_rthpos);
	uint signaled()			const 	{return smask;}
	uint signaled(uint _s)	const;
	
	//setters:
	void id(ulong _fullid);
	void id(ulong _srvid, ulong _ind);
	void setThread(uint _thrid, uint _thrpos);
		
	int state()	const 	{return crtstate;}
	/**
	 * Returns true if the signal should raise the object ASAP
	 * @param _smask 
	 * @return 
	 */
	int signal(uint _smask);
	virtual int signal(CmdPtr<Command> &_cmd);
	virtual int execute();
	virtual int execute(ulong _evs, TimeSpec &_rtout);
	virtual int accept(Visitor &_roi);
	virtual void mutex(Mutex *_pmut);
protected:
	void state(int _st);
	ulong grabSignalMask(ulong _leave = 0);
	friend class Service;
	friend class ObjPtrBase;
	virtual ~Object();//only objptr base can destroy an object
private:
	ulong			fullid;
	uint			smask;
	volatile uint	thrid;//the current thread which (may) execute(s) the object
	volatile uint	thrpos;//
	short			usecnt;//
	short			crtstate;// < 0 -> must die
};
inline void Object::getThread(uint &_rthid, uint &_rthpos){
	//which is better:
	//new thread id and old pos, or
	//new pos and old thread id
	_rthpos = thrpos;
	_rthid  = thrid;
}
inline void Object::setThread(uint _thrid, uint _thrpos){
	thrid  = _thrid;
	thrpos = _thrpos;
}
inline ulong Object::computeIndex(ulong _fullid){
	return _fullid & INDEXMASK;
}
inline ulong Object::computeServiceId(ulong _fullid){
	return _fullid >> INDEXBITCNT;
}

inline ulong Object::grabSignalMask(ulong _leave){
	ulong sm = smask;
	smask = sm & _leave;
	return sm;
}
inline uint Object::signaled(uint _s) const{
	return smask & _s;
}
inline void Object::id(ulong _fullid){
	fullid = _fullid;
}
inline void Object::id(ulong _srvid, ulong _ind){
	fullid = (_srvid << INDEXBITCNT) | _ind;
}
inline void Object::state(int _st){
	crtstate = _st;//if state < 0 the object can be destroyed
}

}//namespace

#endif
