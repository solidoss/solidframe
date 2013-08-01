// frame/src/execscheduler.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/schedulerbase.hpp"
#include "frame/selectorbase.hpp"
#include "frame/manager.hpp"
#include "utility/list.hpp"

namespace solid{
namespace frame{

struct SchedulerBase::Data{
	Data():crtidx(0){}
	typedef List<uint16>				UInt16ListT;
	typedef std::pair<
		SelectorBase*,
		UInt16ListT::iterator
		>								SelectorPairT;
	typedef std::vector<SelectorPairT>	SelectorPairVectorT;
	
	SelectorPairVectorT		selvec;
	UInt16ListT				idxlst;
	uint16					crtidx;
};


SchedulerBase::SchedulerBase(
	Manager &_rm,
	uint16 _startwkrcnt,
	uint16 _maxwkrcnt,
	const IndexT &_selcap
):rm(_rm), d(*(new Data)), startwkrcnt(_startwkrcnt), maxwkrcnt(_maxwkrcnt), crtwkrcnt(0), selcap(_selcap){
	if(maxwkrcnt == 0) maxwkrcnt = 1;
}
/*virtual*/ SchedulerBase::~SchedulerBase(){
	delete &d;
}
bool SchedulerBase::prepareThread(SelectorBase *_ps){
	if(rm.prepareThread(_ps)){
		if(_ps){
			safe_at(d.selvec, _ps->id()) = Data::SelectorPairT(_ps, d.idxlst.insert(d.idxlst.end(), _ps->id()));
		}
		return true;
	}else{
		return false;
	}
}
void SchedulerBase::unprepareThread(SelectorBase *_ps){
	if(_ps){
		d.selvec[_ps->id()].first = NULL;
		d.selvec[_ps->id()].second = d.idxlst.end();
	}
	rm.unprepareThread(_ps);
}
bool SchedulerBase::tryRaiseOneSelector()const{
	if(d.idxlst.size()){
		d.selvec[d.idxlst.back()].first->raise();
		return true;
	}
	return false;
}
void SchedulerBase::raiseOneSelector(){
	if(d.selvec.empty()) return;
	uint cnt(d.selvec.size());
	while(cnt-- && !d.selvec[d.crtidx].first){
		++d.crtidx;
		d.crtidx %= d.selvec.size();
	}
	if(d.selvec[d.crtidx].first){
		d.selvec[d.crtidx].first->raise();
		++d.crtidx;//try use another selector
		d.crtidx %= d.selvec.size();
	}
}
void SchedulerBase::markSelectorFull(SelectorBase &_rs){
	if(d.selvec[_rs.id()].second != d.idxlst.end()){
		d.idxlst.erase(d.selvec[_rs.id()].second);
		d.selvec[_rs.id()].second = d.idxlst.end();
	}
}
void SchedulerBase::markSelectorNotFull(SelectorBase &_rs){
	if(d.selvec[_rs.id()].second == d.idxlst.end()){
		d.selvec[_rs.id()].second = d.idxlst.insert(d.idxlst.end(), _rs.id());
	}
}
void SchedulerBase::doStop(){
	for(Data::SelectorPairVectorT::const_iterator it(d.selvec.begin()); it != d.selvec.end(); ++it){
		if(it->first){
			it->first->raise();
		}
	}
}

}//namespace frame
}//namespace solid
