// utility/src/list.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "utility/list.hpp"

namespace solid{

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

}//namespace solid
