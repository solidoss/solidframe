/* Declarations file stack.h
	
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

#ifndef UTILITY_STACK_H
#define UTILITY_STACK_H
#include <cstdlib>
#include "system/convertors.h"
#include <cassert>


//! A simple and fast stack with interface similar to std one
/*!
	The advantages are:
	- twice faster then the std one
	- while pushing new objects, the allready pushed are not relocated
	in memory (no reallocation is performed)
*/
template <class T, size_t NBits = 5>
class Stack{
	enum{
		NodeMask = BitsToMsk(NBits),
		NodeSize = BitsToCnt(NBits)
	};
	struct Node{
		Node(Node *_pprev = NULL): prev(_pprev){}
		Node	*prev;
		char	data[NodeSize * sizeof(T)];
	};
public:
	typedef T& 			reference;
	typedef T const&	const_reference;
public:
	Stack():sz(0),p(NULL),ptn(NULL){}
	~Stack(){
		while(sz) pop();
		while(ptn){
			Node *tn = ptn->prev;
			delete ptn;
			ptn = tn;
		}
	}
	bool empty()const{return !sz;}
	size_t size()const{return sz;}
	void push(const T &_t){
		if((sz) & NodeMask) ++p;
		else p = pushNode(p);
		
		++sz;
		new(p) T(_t);
	}
	reference top(){return *p;}
	const_reference top()const{return *p;}
	void pop(){
		p->~T();
		if((--sz) & NodeMask) --p;
		else p = popNode(p);
	}
private:
	T *pushNode(void *_p){//_p points to 
		Node *pn = _p ? (Node*)(((char*)_p) - NodeSize * sizeof(T) + sizeof(T) - sizeof(Node*)): NULL;
		if(ptn){
			Node *tn = pn;
			pn = ptn;
			ptn = ptn->prev;
			pn->prev = tn;
		}else{
			pn = new Node(pn);
		}
		return (T*)pn->data;
	}
	T *popNode(void *_p){
		//assert(_p);
		Node *pn = ((Node*)(((char*)_p) - sizeof(Node*)));
		Node *ppn = pn->prev; 
		assert(pn != ptn);
		pn->prev = ptn; ptn = pn;//cache the node
		if(ppn){
			return (T*)(ppn->data + (NodeSize * sizeof(T) - sizeof(T)));
		}else{
			assert(!sz);
			return NULL;
		}
	}
private:
	size_t 		sz;
	T			*p;
	Node		*ptn;
};

#endif
