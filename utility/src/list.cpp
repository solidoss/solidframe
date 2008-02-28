/* Implementation file list.cpp
	
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

#include "list.hpp"

Link* ListBase::doPushBack(Link *_pl){
	++sz;
	lend.pprev->pnext = _pl;
	_pl->pprev = lend.pprev;
	lend.pprev = _pl;
	_pl->pnext = &lend;
	return _pl;
}
Link* ListBase::doPushFront(Link *_pl){
	++sz;
	lend.pnext->pprev = _pl;
	_pl->pnext = lend.pnext;
	lend.pnext = _pl;
	_pl->pprev = &lend;
	return _pl;
}
Link* ListBase::doPushBack(){
	Link* pl = ptop;
	ptop = ptop->pnext;
	return doPushBack(pl);
}
Link* ListBase::doPushFront(){
	Link* pl = ptop;
	ptop = ptop->pnext;
	return doPushFront(pl);
}



Link* ListBase::doErase(Link *_pl){
	--sz;
	Link *pn = _pl->pnext;
	_pl->pprev->pnext = pn;
	pn->pprev = _pl->pprev;
	_pl->pnext = ptop;
	cassert(_pl != &lend);
	ptop = _pl;
	return pn;
}

Link* ListBase::doInsert(Link* _at, Link *_what){
	++sz;
	_at->pprev->pnext = _what;
	_what->pprev = _at->pprev;
	_at->pprev = _what;
	_what->pnext = _at;
	return _what;
}
Link* ListBase::doInsert(Link* _at){
	Link* pl = ptop;
	ptop = ptop->pnext;
	return doInsert(_at, pl);
}

void ListBase::doClear(){
	if(sz){
		lend.pprev->pnext = ptop;
		ptop = lend.pnext;
		lend.pprev = lend.pnext = &lend;
		sz = 0;
	}
}
