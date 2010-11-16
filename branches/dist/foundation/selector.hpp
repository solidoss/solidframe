#ifndef FOUNDATION_SELECTOR_HPP
#define FOUNDATION_SELECTOR_HPP

#include "foundation/object.hpp"

namespace foundation{

class Selector{
protected:
	void associateObjectToCurrentThread(Object &_robj);
	void setObjectThread(Object &_robj, uint32 _thrid, uint32 _thrpos);
	int executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout);
};

inline void Selector::associateObjectToCurrentThread(Object &_robj){
	_robj.associateToCurrentThread();
}
inline void Selector::setObjectThread(Object &_robj, uint32 _thrid, uint32 _thrpos){
	_robj.setThread(_thrid, _thrpos);
}
inline int Selector::executeObject(Object &_robj, ulong _evs, TimeSpec &_rtout){
	return _robj.execute(_evs, _rtout);
}

}//namespace foundation

#endif

