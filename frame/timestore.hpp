// frame/timestore.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#ifndef SOLID_FRAME_TIME_STORE_HPP
#define SOLID_FRAME_TIME_STORE_HPP

#include <vector>
#include "system/timespec.hpp"

namespace solid{
namespace frame{

template <typename V>
class TimeStore{
public:
	typedef V	ValueT;
	
	TimeStore(const size_t _cp = 0){
		tv.reserve(_cp);
		mint = TimeSpec::maximum;
	}
	~TimeStore(){}
	
	size_t size()const{
		return tv.size();
	}
	
	size_t push(TimeSpec const& _rt, ValueT const &_rv){
		const size_t rv = tv.size();
		tv.push_back(TimePairT(_rt, _rv));
		if(_rt < mint){
			mint = _rt;
		}
		return rv;
	}
	
	template <typename F>
	void pop(const size_t _idx, F const &_rf){
		const size_t oldidx = tv.size() - 1;
		tv[_idx] = tv.back();
		tv.pop_back();
		if(_idx < tv.size()){
			_rf(tv[_idx].second, _idx, oldidx);
		}
	}
	
	ValueT change(const size_t _idx, TimeSpec const& _rt){
		tv[_idx].first = _rt;
		return tv[_idx].second;
	}
	
	template <typename F1, typename F2>
	void pop(TimeSpec const& _rt, F1 const &_rf1, F2 const &_rf2){
		
		TimeSpec crtmin = TimeSpec::maximum;
		for(size_t i = 0; i < tv.size();){
			TimePairT const	&rtp = tv[i];
			if(rtp.first > _rt){
				++i;
				if(rtp.first < crtmin){
					crtmin = rtp.first;
				}
			}else{
				_rf1(i, rtp.second);
				
				const size_t oldidx = tv.size() - 1;
				tv[i] = tv.back();
				tv.pop_back();
				if(i < tv.size()){
					_rf2(tv[i].second, i, oldidx);
				}
			}
		}
		
		mint = crtmin;
	}
	TimeSpec const & next()const{
		return mint;
	}
private:
	typedef std::pair<TimeSpec, ValueT>		TimePairT;
	typedef std::vector<TimePairT>			TimeVectorT;
	TimeVectorT		tv;
	TimeSpec		mint;
};

}//namespace frame
}//namespace solid

#endif