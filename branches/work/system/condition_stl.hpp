#ifndef SYSTEM_CONDITION_STL_HPP
#define SYSTEM_CONDITION_STL_HPP


#if	defined(USTLMUTEX)

#include <condition_variable>
#include <chrono>
#include "system/mutex.hpp"
#include "system/timespec.hpp"

struct Condition: std::condition_variable{
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
		std::condition_variable::wait(_lock);
		return 0;
	}
	//! Wait for a signal a certain amount of time
	int wait(Locker<Mutex> &_lock, const TimeSpec &_ts){
		std::chrono::milliseconds ms(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(_ts.seconds()) +
			std::chrono::nanoseconds(_ts.nanoSeconds())
		));
		std::chrono::system_clock::time_point time_limit(
			ms
		);
		std::cv_status stat = wait_until(_lock, time_limit);
		if(stat == std::cv_status::timeout){
			return 110;
		}else{
			return 0;
		}
	}
};

#endif

#endif
