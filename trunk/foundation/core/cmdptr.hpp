/* Declarations file cmdptr.hpp
	
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

#ifndef CLIENT_SERVER_CMDPTR_HPP
#define CLIENT_SERVER_CMDPTR_HPP

#include "common.hpp"

namespace foundation{

class Command;

class CmdPtrBase{
protected:
	void clear(Command *_pcmd);
	void use(Command *_pcmd);

};
//! An autoptr like smartpointer for commands
template <class T>
class CmdPtr:CmdPtrBase{
public:
	typedef CmdPtr<T>	CmdPtrTp;
	typedef T			CmdTp;
	
	explicit CmdPtr(CmdTp *_pcmd = NULL):pcmd(_pcmd){
		if(_pcmd) use(static_cast<Command*>(_pcmd));
	}
	CmdPtr(const CmdPtrTp &_rcp):pcmd(_rcp.release()){}
	~CmdPtr(){
		if(pcmd){CmdPtrBase::clear(static_cast<Command*>(pcmd));}
	}
	CmdTp* release() const{
		CmdTp *tmp = pcmd;
		pcmd = NULL;return tmp;
	}
	CmdPtrTp& operator=(const CmdPtrTp &_rcp){
		if(pcmd) clear();
		pcmd = _rcp.release();
		return *this;
	}
	CmdPtrTp& operator=(CmdTp *_pcmd){
		clear();ptr(_pcmd);	return *this;
	}
	CmdTp& operator*()const {return *pcmd;}
	CmdTp* operator->()const{return pcmd;}
	CmdTp* ptr() const		{return pcmd;}
	//operator bool () const	{return pcmd;}
	bool operator!()const	{return !pcmd;}
	operator bool()	const	{return pcmd != NULL;}
	void clear(){if(pcmd) CmdPtrBase::clear(static_cast<Command*>(pcmd));pcmd = NULL;}
protected:
	void ptr(CmdTp *_pcmd){
		pcmd = _pcmd;use(static_cast<Command*>(pcmd));
	}
private:
	mutable CmdTp *pcmd;	
};

}

#endif
