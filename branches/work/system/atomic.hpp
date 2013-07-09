#ifndef SOLID_SYSTEM_ATOMIC_HPP
#define SOLID_SYSTEM_ATOMIC_HPP

#include "config.h"

#ifdef HAS_STDATOMIC
#define ATOMIC_NS std
#else
#define ATOMIC_NS boost
#endif

#ifdef HAS_STDATOMIC
#include <atomic>
#else
#include "boost/atomic.hpp"
#endif

#endif