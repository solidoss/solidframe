#ifndef DCHAT_STRESS_SERVICE_HPP
#define DCHAT_STRESS_SERVICE_HPP

#include "frame/service.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aioselector.hpp"
#include "system/socketaddress.hpp"
#include <vector>

typedef solid::frame::Scheduler<solid::frame::aio::Selector>	AioSchedulerT;
typedef std::vector<solid::SocketAddressInet>					AddressVectorT;

struct StartData{
	StartData():concnt(0), reconnectcnt(0), reconnectsleepmsec(1000){}
	size_t		concnt;
	size_t		reconnectcnt;
	size_t		reconnectsleepmsec;
};

class Service: public solid::Dynamic<Service, solid::frame::Service>{
public:
	static void registerMessages();
	
	Service(
		solid::frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const AddressVectorT &_raddrvec
	):BaseT(_rm), rsched(_rsched), raddrvec(_raddrvec){
	}
	~Service(){}
	
	void start(const StartData &_rsd, const bool _async = true);
	
private:
	friend class Connection;
	struct StartThread;
	void doStart(const StartData &_rsd);
private:
	AioSchedulerT			&rsched;
	const AddressVectorT	&raddrvec;
};




#endif
