#ifndef SYS_CASSERT_HPP
#define SYS_CASSERT_HPP

#ifdef UASSERT
#include <cassert>
#define cassert(a) assert(a)
#else
#define cassert(a)
#endif

#endif
