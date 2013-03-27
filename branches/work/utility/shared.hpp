#ifndef UTILITY_SHARED_HPP
#define UTILITY_SHARED_HPP

namespace solid{

struct Mutex;

struct Shared{
	static Mutex& mutex(void *_pv);
	Shared();
	Mutex& mutex();
};

}//namespace solid

#endif
