#ifndef SYS_CASSERT_H
#define SYS_CASSERT_H

#ifdef UASSERT
#include <cassert>
#define cassert(a) assert(a)
#else
#define cassert(a)
#endif

#endif
