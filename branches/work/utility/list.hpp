/* Declarations file list.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTILITY_LIST_HPP
#define UTILITY_LIST_HPP

#include "system/cassert.hpp"
#include <stdlib.h>

typedef unsigned short ushort;
typedef unsigned long  ulong;

struct Link{
	Link	*pprev;
	Link	*pnext;
};

class ListBase{
protected:
	ListBase():sz(0),ptop(NULL){
		lend.pprev = lend.pnext = &lend;
	}
	bool isEmpty()const {return !sz;}
	size_t	theSize()const {return sz;}
	Link* doPushBack(Link *_pl);
	Link* doPushFront(Link *_pl);
	Link* doPushBack();
	Link* doPushFront();
	void doPopBack(){
		doErase(lend.pprev);
	}
	void doPopFront(){
		doErase(lend.pnext);
	}
	Link* doErase(Link *_pl);
	Link* doInsert(Link* _at, Link *_what);
	Link* doInsert(Link* _at);
	void  doClear();
	
	bool cacheEmpty()const{ return ptop == NULL;}

	Link* theBack(){
		return lend.pprev;
	}
	const Link* theBack()const{
		return lend.pprev;
	}
	Link* theFront(){
		return lend.pnext;
	}
	const Link* theFront()const{
		return lend.pnext;
	}
	Link* theEnd(){return &lend;}
	const Link* theEnd()const{return &lend;}
protected:
	size_t	sz;
	Link	*ptop;
	Link	lend;
};


template <class T>
struct ListIterator{
	typedef typename T::value_type	value_type;
	ListIterator(const ListIterator &_li):pl(_li.pl){}
	ListIterator(Link *_pl = NULL):pl(_pl){}
	ListIterator& operator=(const ListIterator &_rit){
		pl = _rit.pl;
		return *this;
	}
	bool operator==(const ListIterator &_rit)const{
		return pl == _rit.pl;
	}
	bool operator!=(const ListIterator &_rit)const{
		return pl != _rit.pl;
	}
	const T& operator*()const{
		return static_cast<T*>(pl)->value();
	}
	value_type& operator*(){
		return static_cast<T*>(pl)->value();
	}
	ListIterator &operator++(){
		pl = pl->pnext;
		return *this;
	}
	ListIterator &operator--(){
		pl = pl->pprev;
		return *this;
	}
	Link	*pl;
};

template <class T>
struct ListConstIterator{
	typedef typename T::value_type	value_type;
	typedef ListIterator<T>			iterator;
	ListConstIterator(const ListConstIterator &_li):pl(_li.pl){}
	ListConstIterator(const iterator &_li):pl(_li.pl){}
	ListConstIterator(Link *_pl = NULL):pl(_pl){}
	ListConstIterator& operator=(const ListConstIterator &_rit){
		pl = _rit.pl;
		return *this;
	}
	bool operator==(const ListConstIterator &_rit)const{
		return pl == _rit.pl;
	}
	bool operator!=(const ListConstIterator &_rit)const{
		return pl != _rit.pl;
	}
	const value_type& operator*()const{
		return static_cast<const T*>(pl)->value();
	}
	ListConstIterator &operator++(){
		pl = pl->pnext;
		return *this;
	}
	ListConstIterator &operator--(){
		pl = pl->pprev;
		return *this;
	}
private:
	const Link	*pl;
};


/*
Front<->Node<->Node<->..<->Back
pprev<-node->pnext
*/
template <class T>
class List: protected ListBase{
protected:
	struct Node: Link{
		typedef T value_type;
		Node(const T &_rt){
			new(data) value_type(_rt);
		}
		Node(){}
		void destroy(){
			reinterpret_cast<value_type*>(data)->~value_type();
		}
		T& create(const T &_rt){
			return *(new(data) value_type(_rt));
		}
		T& create(){
			return *(new(data) value_type);
		}
		T& value(){return *reinterpret_cast<value_type*>(data);}
		char	data[sizeof(value_type)];
	};
public:
	typedef ListIterator<Node>	iterator;
	typedef ListConstIterator<Node> const_iterator;
	typedef ListIterator<Node>	reverse_iterator;
	typedef ListConstIterator<Node> const_reverse_iterator;
public:
	List(){}
	~List(){
		while(size()){
			pop_back();
		}
		if(ptop == &lend) ptop = ptop->pnext;
		Node *pn;
		while(ptop){
			pn = static_cast<Node*>(ptop);
			ptop = ptop->pnext;
			delete pn;
		}
	}
	void push_back(const T& _rt){
		if(this->cacheEmpty())
			doPushBack(new Node(_rt));
		else
			static_cast<Node*>(doPushBack())->create(_rt);
	}
	T& push_back(){
		if(this->cacheEmpty())
			return static_cast<Node*>(doPushBack(new Node))->value();
		else
			return static_cast<Node*>(doPushBack())->create();
	}
	
	void push_front(const T& _rt){
		if(this->cacheEmpty())
			doPushFront(new Node(_rt));
		else
			static_cast<Node*>(doPushFront())->create(_rt);
	}
	T& push_front(){
		if(this->cacheEmpty())
			return static_cast<Node*>(doPushFront(new Node))->value();
		else
			return static_cast<Node*>(doPushFront())->create();
	}
	//pops:
	void pop_back(){
		static_cast<Node*>(theBack())->destroy();
		doPopBack();
	}
	void pop_front(){
		static_cast<Node*>(theFront())->destroy();
		doPopFront();
	}
	//back
	T& back(){
		return static_cast<Node*>(theBack())->value();
	}
	const T& back() const{
		return static_cast<Node*>(theBack())->value();
	}
	//front
	T& front(){
		return static_cast<Node*>(theFront())->value();
	}
	const T& front() const{
		return static_cast<Node*>(theFront())->value();
	}
	//iterators:
	const_iterator begin()const{
		return const_iterator(theFront());
	}
	iterator begin(){
		return iterator(theFront());
	}
	iterator end(){
		return iterator(theEnd());
	}
	const_iterator end() const {
		return const_iterator(theEnd());
	}
	reverse_iterator rbegin(){
	}
	const_reverse_iterator rbegin()const{
	}
	reverse_iterator rend(){
	}
	const_reverse_iterator rend()const{
	}
	iterator insert(iterator _it, const T &_rt){
		if(cacheEmpty())
			return iterator(doInsert(_it.pl,new Node(_rt)));
		else{
			Node *pn = static_cast<Node*>(doInsert(_it.pl));
			pn->create(_rt);
			return iterator(pn);
		}
	}
	iterator insert(iterator _it){
		if(cacheEmpty())
			return iterator(doInsert(_it.pl,new Node));
		else
			return iterator(doInsert(_it.pl));
	}
	iterator erase(iterator _it){
		return iterator(doErase(_it.pl));
	}
	void clear(){
		doClear();
	}
	//tools:
	bool empty()const{return isEmpty();}
	size_t size()const{return theSize();}
};

#endif

