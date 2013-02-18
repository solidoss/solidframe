#include "frame/manager.hpp"
#include "system/thread.hpp"

int main(int argc, char *argv[]){
	/*solid::*/Thread::init();
	{
		solid::Manager			m;
		solid::aio::SchedulerT	aiosched(m);
		
		{
			Listener::SharedPointerT objptr(new Listener(m, aiosched,/*sd, pctx*/));
			m.registerObject(objptr);
			aiosched.push(objptr);
		}
		
		wait_sig_term();
		m.stop(false);
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}

