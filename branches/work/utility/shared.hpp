#ifndef UTILITY_SHARED_HPP
#define UTILITY_SHARED_HPP

class Mutex;

struct Shared{
	static Mutex& mutex(void *_pv);
	Shared();
	Mutex& mutex();
};

#endif
