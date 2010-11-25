#ifndef FOUNDATION_SELECTOR_BASE_HPP
#define FOUNDATION_SELECTOR_BASE_HPP

#include "foundation/object.hpp"

namespace foundation{

class SelectorBase{
protected:
	void associateObjectToCurrentThread(Object &_robj);
	void setObjectThread(Object &_robj, uint32 _thrid, uint32 _objidx);
	int executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout);
	virtual void raise(uint32 _objidx = 0) = 0;
};

inline void SelectorBase::associateObjectToCurrentThread(Object &_robj){
	_robj.associateToCurrentThread();
}
inline void SelectorBase::setObjectThread(Object &_robj, uint32 _thrid, uint32 _objidx){
	_robj.setThread(_thrid, _objidx);
}
inline int SelectorBase::executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout){
	return _robj.execute(_evs, _rtout);
}

}//namespace foundation

#endif

