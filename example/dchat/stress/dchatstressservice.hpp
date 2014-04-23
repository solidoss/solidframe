#ifndef DCHAT_STRESS_SERVICE_HPP
#define DCHAT_STRESS_SERVICE_HPP

#include "frame/service.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aioselector.hpp"
#include "system/socketaddress.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/timespec.hpp"


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
		const AddressVectorT &_raddrvec,
		const size_t _idx
	):BaseT(_rm), rsched(_rsched), raddrvec(_raddrvec), idx(_idx){
		expect_create_cnt = 0;
		actual_create_cnt = 0;
	
		expect_connect_cnt = 0;
		actual_connect_cnt = 0;
		
		expect_login_cnt = 0;
		actual_login_cnt = 0;
		
		expect_receive_cnt = 0;
		actual_receive_cnt = 0;
	}
	~Service(){}
	
	void start(const StartData &_rsd, const bool _async = true);
	
	void send(const size_t _msgrow, const size_t _sleepms, const size_t _cnt);
	
	void waitCreate();
	void waitConnect();
	void waitLogin();
	void waitReceive();
	
	solid::TimeSpec startTime();
	solid::TimeSpec sendTime();
	size_t index()const{
		return idx;
	}
	
private:
	friend class Connection;
	struct StartThread;
	void doStart(const StartData &_rsd);
	void onCreate();
	void onConnect();
	void onReceive();
	void onLogin();
	void onReceive(const size_t _sz);
	void onReceiveDone();
	
private:
	AioSchedulerT			&rsched;
	const AddressVectorT	&raddrvec;
	solid::Mutex			mtx;
	solid::Condition		cnd;
	
	size_t					expect_create_cnt;
	size_t					actual_create_cnt;
	
	size_t					expect_connect_cnt;
	size_t					actual_connect_cnt;
	
	size_t					expect_login_cnt;
	size_t					actual_login_cnt;
	
	size_t					expect_receive_cnt;
	size_t					actual_receive_cnt;
	solid::TimeSpec			start_time;
	solid::TimeSpec			send_time;
	bool					waiting;
	solid::uint64			recv_sz;
	size_t					recv_cnt;
	solid::TimeSpec			last_time;
	const size_t			idx;
};




#endif
