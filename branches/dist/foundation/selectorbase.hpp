#ifndef FOUNDATION_SELECTOR_BASE_HPP
#define FOUNDATION_SELECTOR_BASE_HPP

#include "foundation/object.hpp"

namespace foundation{

class SelectorBase{
public:
	uint32 id()const{
		return selid;
	}
	virtual void raise(uint32 _objidx = 0) = 0;
protected:
	void associateObjectToCurrentThread(Object &_robj);
	void setObjectThread(Object &_robj, uint32 _objidx);
	int executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout);
	void id(uint32 _id);
private:
	uint32	selid;//given by manager
};

inline void SelectorBase::associateObjectToCurrentThread(Object &_robj){
	_robj.associateToCurrentThread();
}
inline void SelectorBase::setObjectThread(Object &_robj, uint32 _objidx){
	_robj.setThread(selid, _objidx);
}
inline int SelectorBase::executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout){
	return _robj.execute(_evs, _rtout);
}

inline void SelectorBase::id(uint32 _id){
	selid = _id;
}

}//namespace foundation

#endif

