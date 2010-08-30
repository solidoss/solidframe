#ifndef SYS_CASSERT_HPP
#define SYS_CASSERT_HPP

#ifdef UASSERT
#include <cassert>
#define cassert(a) assert(a)
#else
#define cassert(a)
#endif



template <bool B>
struct static_test;

template <>
struct static_test<true>{
	static void ok(){
	}
};

template <>
struct static_test<false>{
};


#define cstatic_assert(e) static_test<(e)>::ok()

#endif
