#ifndef SYSTEM_CONDITION_HPP
#define SYSTEM_CONDITION_HPP

struct TimeSpec;
struct Mutex;
class Condition{
public:
	Condition();
	~Condition();
	int signal();
	int broadcast();
	int wait(Mutex &_mut);
	int wait(Mutex &_mut, const TimeSpec &_ts);
private:
	pthread_cond_t cond;
};

#ifdef UINLINES
#include "src/condition.ipp"
#endif


#endif
