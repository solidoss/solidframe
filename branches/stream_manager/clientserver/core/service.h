/* Declarations file service.h
	
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

#ifndef CS_SERVICE_H
#define CS_SERVICE_H

#include "system/synchronization.h"

#include "utility/sharedcontainer.h"

#include "object.h"

namespace clientserver{

class Server;
class Visitor;
class ObjectVector;
class IndexStack;

class Service: public Object{
public:
	enum States{Running, Stopping, Stopped};
	virtual ~Service();
	/**
	 * Added the Server parameter too because
	 * the function may be called also from a non pool thread,
	 * so the Server will not be available from thread static.
	 */
	int signal(Object &_robj, Server &_rsrv, ulong _sigmask);
	int signal(ulong _fullid, ulong _uid, Server &_rsrv, ulong _sigmask);
	
	int signal(Object &_robj, Server &_rsrv, CmdPtr<Command> &_cmd);
	int signal(ulong _fullid, ulong _uid, Server &_rsrv, CmdPtr<Command> &_cmd);
	
	void signalAll(Server &_rsrv, ulong _sigmask);
	void signalAll(Server &_rsrv, CmdPtr<Command> &_cmd);
	
	void visit(Server &_rsrv, Visitor &_rov);
	
	Mutex& mutex(Object &_robj);
	ulong  uid(Object &_robj)const;
	
	void mutex(Mutex *_pmut);
	Mutex* mutex()const{return mut;}
	virtual int start(Server &_rsrv);
	virtual int stop(Server &_rsrv, bool _wait = true);
	virtual int execute();
	
protected:
	typedef SharedContainer<Mutex>	MutexContainer;
	int insert(Object &_robj, ulong _srvid);
	void remove(Object &_robj);
	Mutex& mutex(ulong _fullid, ulong _uid);
	Object* object(ulong _fullid, ulong _uid);
	
	void doSignalAll(Server &_rsrv, ulong _sigmask);
	void doSignalAll(Server &_rsrv, CmdPtr<Command> &_cmd);
	int doInsert(Object &_robj, ulong _srvid);
	Service(int _objpermutbts = 6, int _mutrowsbts = 8, int _mutcolsbts = 8);
	//Service(const Service &):state(Stopped),objv(*((ObjectVector*)NULL)),
	//							indq(*((IndexQueue*)NULL)){}
	const Service& operator=(const Service &){return *this;}
	
	Mutex& mutex(unsigned i);
	
	ObjectVector		&objv;
	IndexStack			&inds;
	Condition			cond;
	Mutex				*mut;
	unsigned			objcnt;//object count
	MutexContainer		mutpool;
};


}//namespace clientserver

#endif
