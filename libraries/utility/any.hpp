// utility/any.hpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_ANY_HPP
#define UTILITY_ANY_HPP

#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <utility>


namespace solid{

namespace impl{

struct AnyValueBase{
	virtual ~AnyValueBase();
	virtual const void* get()const = 0;
	virtual void* get() = 0;
	virtual AnyValueBase* copyTo(void *_pd, const size_t _sz) const = 0;
	virtual AnyValueBase* moveTo(void *_pd, const size_t _sz, const bool _uses_data) = 0;
};


template <class T>
struct AnyValue: AnyValueBase{
	explicit AnyValue(const T &_rt):value_(_rt){}
	explicit AnyValue(T &&_rt):value_(std::move(_rt)){}
	
	
	const void* get()const override{
		return &value_;
	}
	
	void* get()override{
		return &value_;
	}
	
	AnyValueBase* copyTo(void *_pd, const size_t _sz) const override{
		if(sizeof(AnyValue<T>) <= _sz){
			return new(_pd) AnyValue<T>{value_};
		}else{
			return new AnyValue<T>{value_};
		}
	}
	
	AnyValueBase* moveTo(void *_pd, const size_t _sz, const bool _uses_data)override{
		if(sizeof(AnyValue<T>) <= _sz){
			return new(_pd) AnyValue<T>{std::move(value_)};
		}else if(not _uses_data){//the pointer was allocated
			return this;
		}else{
			return new AnyValue<T>{std::move(value_)};
		}
	}
	
	T	value_;
};

}//namespace impl


class AnyBase{
public:
	static constexpr size_t min_data_size = sizeof(impl::AnyValue<void*>);
protected:
	
	AnyBase():pvalue_(nullptr){}
	
	void doMoveFrom(AnyBase &_ranybase, void *_pv, const size_t _sz, const bool _uses_data){
		if(_ranybase.pvalue_){
			pvalue_ = _ranybase.pvalue_->moveTo(_pv, _sz, _uses_data);
			if(pvalue_ == _ranybase.pvalue_){
				_ranybase.pvalue_ = nullptr;
			}
		}
	}
	
	void doCopyFrom(const AnyBase &_ranybase, void *_pv, const size_t _sz){
		if(_ranybase.pvalue_){
			pvalue_ = _ranybase.pvalue_->copyTo(_pv, _sz);
		}
	}
	
	impl::AnyValueBase	*pvalue_;
};

template <size_t DataSize = 0>
class Any;

template <size_t DataSize>
class Any: protected AnyBase{
public:
	using ThisT = Any<DataSize>;
	
	
	Any(){}
	
	
	template <size_t DS>
	explicit Any(const Any<DS> &_rany){
		doCopyFrom(_rany, data_, DataSize);
	}
	
	template <size_t DS>
	explicit Any(Any<DS> &&_rany){
		doMoveFrom(_rany, data_, DataSize, _rany.usesData());
		_rany.clear();
	}
	
	Any(const ThisT &_rany){
		doCopyFrom(_rany, data_, DataSize);
	}
	
	Any(ThisT &&_rany){
		doMoveFrom(_rany, data_, DataSize, _rany.usesData());
		_rany.clear();
	}
	
	
	template <typename T>
	explicit Any(const T &_rt, bool _dummy){
		reset(_rt);
	}
	
	template <typename T>
	explicit Any(T &&_rt, bool _dummy){
		reset(std::move(_rt));
	}
	
	
	~Any(){
		clear();
	}
	
	template <typename T>
	T* cast(){
		if(pvalue_){
			const std::type_info &req_type = typeid(impl::AnyValue<T>);
			const std::type_info &val_type = typeid(*pvalue_);
			if(std::type_index(req_type) == std::type_index(val_type)){
				return reinterpret_cast<T*>(pvalue_->get());
			}
		}
		return nullptr;
	}
	
	template <typename T>
	const T* cast()const{
		if(pvalue_){
			const std::type_info &req_type = typeid(impl::AnyValue<T>);
			const std::type_info &val_type = typeid(*pvalue_);
			if(std::type_index(req_type) == std::type_index(val_type)){
				return reinterpret_cast<const T*>(pvalue_->get());
			}
		}
		return nullptr;
	}
	
	bool empty()const{
		return pvalue_ == nullptr;
	}
	
	void clear(){
		if(usesData()){
			pvalue_->~AnyValueBase();
		}else{
			delete pvalue_;
		}
		pvalue_ = nullptr;
	}
	
	ThisT& operator=(const ThisT &_rany){
		if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
			clear();
			doCopyFrom(_rany, data_, DataSize);
		}
		return *this;
	}
	
	ThisT& operator=(ThisT &&_rany){
		if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
			clear();
			doMoveFrom(_rany, data_, DataSize, _rany.usesData());
			_rany.clear();
		}
		return *this;
	}
	
	template <size_t DS>
	ThisT& operator=(const Any<DS> &_rany){
		if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
			clear();
			doCopyFrom(_rany, data_, DataSize);
		}
		return *this;
	}
	
	template <size_t DS>
	ThisT& operator=(Any<DS> &&_rany){
		if(static_cast<const void*>(this) != static_cast<const void*>(&_rany)){
			clear();
			doMoveFrom(_rany, data_, DataSize, _rany.usesData());
			_rany.clear();
		}
		return *this;
	}
	
	template <typename T>
	void reset(const T &_rt){
		clear();
		if(sizeof(impl::AnyValue<T>) <= DataSize){
			pvalue_ = new(data_) impl::AnyValue<T>{_rt};
		}else{
			pvalue_ = new impl::AnyValue<T>{_rt};
		}
		return *this;
	}
	
	template <typename T>
	void reset(T &&_rt){
		clear();
		if(sizeof(impl::AnyValue<T>) <= DataSize){
			pvalue_ = new(data_) impl::AnyValue<T>(std::move(_rt));
		}else{
			pvalue_ = new impl::AnyValue<T>(std::move(_rt));
		}
	}
	
	bool usesData()const{
		return reinterpret_cast<const void*>(pvalue_) == reinterpret_cast<const void*>(data_);
	}
	
private:
	char				data_[DataSize];
};


template <size_t DS, typename T>
Any<DS> any_create(const T &_rt){
	return Any<DS>(_rt, true);
}

template <size_t DS, typename T>
Any<DS> any_create(T &&_rt){
	return Any<DS>(_rt, true);
}


}//namespace solid


#endif