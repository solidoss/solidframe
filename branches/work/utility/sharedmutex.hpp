#ifndef UTILITY_SHARED_MUTEX_STORE_HPP
#define UTILITY_SHARED_MUTEX_STORE_HPP

namespace solid{

struct Mutex;

Mutex &shared_mutex_safe(void *_p);
Mutex &shared_mutex(void *_p);


}//namespace solid

#endif
