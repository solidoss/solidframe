#ifndef UTILITY_MEMORY_HPP
#define UTILITY_MEMORY_HPP

#include "system/common.hpp"

struct EmptyChecker{
	EmptyChecker(const char *_fncname):v(0), fncname(_fncname){}
	~EmptyChecker();
	void add();
	void sub();
private:
	unsigned long v;
	const char * fncname;
};

#ifdef UDEBUG

#ifdef HAVE_SAFE_STATIC
template <class T>
void objectCheck(bool _add, const char *_fncname){
	static EmptyChecker ec(_fncname);
	if(_add)	ec.add();
	else 		ec.sub();
}
#else
template <class T>
void objectCheckStub(bool _add, const char *_fncname){
	static EmptyChecker ec(_fncname);
	if(_add)	ec.add();
	else 		ec.sub();
}

template <class T>
void once_cbk_memory(){
	objectCheckStub<T>(true, "once_cbk_memory");
	objectCheckStub<T>(false, "once_cbk_memory");
}

template <class T>
void objectCheck(bool _add, const char *_fncname){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk_memory<T>, once);
	return objectCheckStub<T>(_add, _fncname);
}


#endif

#else
template <class T>
void objectCheck(bool _add, const char *){
}
#endif


#endif
