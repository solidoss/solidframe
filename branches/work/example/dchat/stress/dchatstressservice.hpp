#ifndef DCHAT_STRESS_SERVICE_HPP
#define DCHAT_STRESS_SERVICE_HPP

#include "frame/service.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aioselector.hpp"

typedef solid::frame::Scheduler<solid::frame::aio::Selector>	AioSchedulerT;

class Service: public solid::Dynamic<Service, solid::frame::Service>{
public:
	static void registerMessages();
	
	Service(
		solid::frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const char *_myname
	):BaseT(_rm), rsched(_rsched){
	}
	~Service(){}
	
private:
	friend class Connection;
private:
	AioSchedulerT	&rsched;
};




#endif
