// solid/utility/stack.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_STACK_HPP
#define UTILITY_STACK_HPP
#include <cstdlib>
#include "solid/system/convertors.hpp"
#include "solid/system/cassert.hpp"
#include <algorithm>

namespace solid{

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
		NodeMask = BitsToMask(NBits),
		NodeSize = BitsToCount(NBits)
	};
	struct Node{
		Node(Node *_pprev = nullptr): prev(_pprev){}
		Node	*prev;
		char	data[NodeSize * sizeof(T)];
	};
public:
	typedef T& 			reference;
	typedef T const&	const_reference;
public:
	Stack():sz(0),p(nullptr),ptn(nullptr){}
	Stack(Stack &&_rs):sz(_rs.sz), p(_rs.p), ptn(_rs.ptn){
		_rs.sz = 0;
		_rs.p = nullptr;
		_rs.ptn = nullptr;
	}
	~Stack(){
		while(sz) pop();
		while(ptn){
			Node *tn = ptn->prev;
			delete ptn;
			ptn = tn;
		}
	}
	
	Stack& operator=(Stack&& _rs){
		sz = _rs.sz;
		p = _rs.p;
		ptn = _rs.ptn;
		
		_rs.sz = 0;
		_rs.p = nullptr;
		_rs.ptn = nullptr;
		return *this;
	}
	
	bool empty()const{return !sz;}
	size_t size()const{return sz;}
	void push(const T &_t){
		if((sz) & NodeMask) ++p;
		else p = pushNode(p);
		
		++sz;
		new(p) T(_t);
	}
	
	void push(T &&_t){
		if((sz) & NodeMask) ++p;
		else p = pushNode(p);
		
		++sz;
		new(p) T(std::move(_t));
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
		Node *pn = _p ? (Node*)(((char*)_p) - NodeSize * sizeof(T) + sizeof(T) - sizeof(Node*)): nullptr;
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
		//cSOLID_ASSERT(_p);
		Node *pn = ((Node*)(((char*)_p) - sizeof(Node*)));
		Node *ppn = pn->prev; 
		SOLID_ASSERT(pn != ptn);
		pn->prev = ptn; ptn = pn;//cache the node
		if(ppn){
			return (T*)(ppn->data + (NodeSize * sizeof(T) - sizeof(T)));
		}else{
			SOLID_ASSERT(!sz);
			return nullptr;
		}
	}
private:
	Stack(const Stack &);
	Stack& operator=(const Stack &);
private:
	size_t 		sz;
	T			*p;
	Node		*ptn;
};

}//namespace solid

#endif
