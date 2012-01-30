#ifndef SYSTEM_CONDITION_BOOST_HPP
#define SYSTEM_CONDITION_BOOST_HPP

#if	defined(UBOOSTMUTEX)

#include "boost/thread/condition_variable.hpp"
#include "boost/chrono.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"

struct Condition: boost::condition_variable{
public:
	Condition(){}
	~Condition(){}
	//! Try to wake one waiting thread
	int signal(){
		this->notify_one();
		return 0;
	}
	//! Try to wake all waiting threads
	int broadcast(){
		this->notify_all();
		return 0;
	}
	//! Wait for a signal
	int wait(Locker<Mutex> &_lock){
		boost::condition_variable::wait(_lock);
		return 0;
	}
	//! Wait for a signal a certain amount of time
	int wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
		boost::system_time time_limit(
			boost::posix_time::from_time_t(0)
		);
		time_limit += boost::posix_time::seconds(static_cast<long>(_ts.seconds()));
		time_limit += boost::posix_time::microseconds(_ts.nanoSeconds()/1000);
		bool stat = timed_wait(_lock, time_limit);
		if(!stat){
			return -1;
		}else{
			return 0;
		}
	}
};

#endif

#endif
