/* Declarations file queue.h
	
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

#ifndef UTILITY_QUEUE_H
#define UTILITY_QUEUE_H
#include <cstdlib>
#include "system/convertors.h"
#include "system/cassert.h"


//! A simple and fast queue with interface similar to std one
/*!
	The advantages are:
	- twice faster then the std one
	- while pushing new objects, the allready pushed are not relocated
	in memory (no reallocation is performed)
*/
template <class T, size_t NBits = 5>
class Queue{
	enum{
		NodeMask = BitsToMsk(NBits),
		NodeSize = BitsToCnt(NBits)
	};
	struct Node{
		Node(Node *_pnext = NULL): next(_pnext){}
		Node	*next;
		char	data[NodeSize * sizeof(T)];
	};
public:
	typedef T& 			reference;
	typedef T const&	const_reference;

public:
	Queue():sz(0),popsz(0),pb(NULL),pf(NULL),ptn(NULL){}
	~Queue(){
		while(sz) pop();
		while(ptn){
			Node *tn = ptn->next;
			delete ptn;
			ptn = tn;
		}
	}
	bool empty()const	{ return !sz;}
	size_t size() const	{ return sz;}
	
	void push(const T &_t){
		if((sz + popsz) & NodeMask) ++pb;
		else pb = pushNode(pb);
		
		++sz;
		new(pb) T(_t);
	}
	reference back(){
		return *pb;
	}
	const_reference back()const{
		return *pb;
	}
	
	void pop(){
		pf->~T();
		--sz;
		cassert(sz < 1000000);
		if((++popsz) & NodeMask) ++pf;
		else{ pf = popNode(pf);popsz = 0;}
	}
	reference front(){
		return *pf;
	}
	const_reference front()const{
		return *pf;
	}
private:
	T* pushNode(void *_p){
		Node *pn = _p ? (Node*)(((char*)_p) - NodeSize * sizeof(T) + sizeof(T) - sizeof(Node*)): NULL;
		if(ptn){
			Node *tn = ptn;
			ptn = ptn->next;
			tn->next = NULL;
			if(pn){
				pn->next = tn;
				return (T*)tn->data;
			}else{
				return (pf = (T*)tn->data);
			}
		}else{
			if(pn){
				pn->next = new Node;
				pn = pn->next;
				return (T*)pn->data;
			}else{
				pn = new Node;
				return (pf = (T*)pn->data);
			}
		}
	}
	T* popNode(void *_p){
		//cassert(_p);
		cassert(_p);
		Node *pn = (Node*)(((char*)_p) - NodeSize * sizeof(T) + sizeof(T) - sizeof(Node*));
		Node *ppn = pn->next;
		pn->next = ptn; ptn = pn;//cache the node
		if(ppn){
			return (T*)(ppn->data);
		}else{
			cassert(!sz);
			pb = NULL;
			return NULL;
		}
	}
private:
	size_t		sz;
	size_t		popsz;
	T			*pb;//back
	T			*pf;//front
	Node		*ptn;//empty nodes
};


#endif
