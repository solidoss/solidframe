// utility/innerlist.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_INNERLIST_HPP
#define UTILITY_INNERLIST_HPP

#include "utility/common.hpp"

namespace solid{

struct InnerLink{
	size_t	prev;
	size_t	next;
	
	void clear(){
		prev = next = InvalidIndex();
	}
	
	InnerLink(
		const size_t _prev = InvalidIndex(),
		const size_t _next = InvalidIndex()
	):prev(_prev), next(_next){}
};

template <size_t Size>
struct InnerNode;

template <size_t Size>
InnerLink& inner_link_accessor(InnerNode<Size> &_node, const size_t _index);

template <size_t Size>
InnerLink const & inner_link_const_accessor(InnerNode<Size> const &_node, const size_t _index);

template <size_t Size>
struct InnerNode{
	enum{
		InnerNodeSize = Size,
	};
	
private:
	template <size_t Sz>
	friend InnerLink& inner_link_accessor(InnerNode<Sz> &_node, const size_t _index);
	template <size_t Sz>
	friend InnerLink const & inner_link_const_accessor(InnerNode<Sz> const &_node, const size_t _index);
	
	InnerLink	links[InnerNodeSize];
};

template <size_t Size>
InnerLink& inner_link_accessor(InnerNode<Size> &_node, const size_t _index){
	return _node.links[_index];
}

template <size_t Size>
InnerLink const & inner_link_const_accessor(InnerNode<Size> const &_node, const size_t _index){
	return _node.links[_index];
}


template <class Vec, size_t Link>
class InnerList{
public:
	typedef typename Vec::value_type ValueT;
	
	InnerList(Vec &_rvec): rvec_(_rvec), size_(0), back_(InvalidIndex()), front_(InvalidIndex()){}
	
	InnerList(InnerList<Vec, Link> &) = delete;
	InnerList(InnerList<Vec, Link> &&) = delete;
	InnerList(
		Vec &_rvec,
		InnerList<Vec, Link> &_rinnerlist
	):rvec_(_rvec), size_(_rinnerlist.size_), back_(_rinnerlist.back_), front_(_rinnerlist.front_){}
	
	void pushBack(const size_t _index){
		InnerLink &rcrt_link = link(_index);
		
		rcrt_link = InnerLink(InvalidIndex(), back_);
		
		if(back_ != InvalidIndex()){
			link(back_).prev = _index;
			back_ = _index;
		}else{
			back_ = _index;
			front_ = _index;
		}
		
		++size_;
	}
	
	ValueT& front(){
		return rvec_[front_];
	}
	
	ValueT const & front()const{
		return rvec_[front_];
	}
	
	size_t frontIndex()const{
		return front_;
	}
	
	void erase(const size_t _index){
		InnerLink &rcrt_link = link(_index);
		
		if(rcrt_link.prev != InvalidIndex()){
			link(rcrt_link.prev).next = rcrt_link.next;
		}else{
			//first message in the list
			back_ = rcrt_link.next;
		}
		
		if(rcrt_link.next != InvalidIndex()){
			link(rcrt_link.next).prev = rcrt_link.prev;
		}else{
			front_ = rcrt_link.prev;
		}
		--size_;
		rcrt_link.clear();
	}
	
	void popFront(){
		erase(front_);
	}

	size_t size()const{
		return size_;
	}
	
	bool empty()const{
		return size_ == 0;
	}
	
	template <class F>
	void forEach(F _f){
		size_t it  = back_;
		
		while(it != InvalidIndex()){
			const size_t crtit = it;
			it = link(it).next;
			
			_f(crtit, rvec_[crtit]);
		}
	}
	
	template <class F>
	void forEach(F _f)const{
		size_t it  = back_;
		
		while(it != InvalidIndex()){
			const size_t crtit = it;
			it = link(it).next;
			
			_f(crtit, rvec_[crtit]);
		}
	}
	
	void fastClear(){
		size_ = 0;
		back_ = front_ = InvalidIndex();
	}
	
	void clear(){
		while(size()){
			popFront();
		}
	}
	
	size_t previousIndex(const size_t _index)const{
		return link(_index).prev;
	}
	bool check()const{
		return not ((back_ == InvalidIndex() or front_ == InvalidIndex()) and back_ == front_);
	}
private:
	InnerLink& link(const size_t _index){
		//typedef Vec::value_type		NodeT;
		return inner_link_accessor(rvec_[_index], Link);
	}
	InnerLink const & link(const size_t _index)const{
		//typedef Vec::value_type		NodeT;
		return inner_link_const_accessor(rvec_[_index], Link);
	}
private:
	Vec		&rvec_;
	size_t	size_;
	size_t	back_;
	size_t	front_;
};

}//namespace solid

#endif
