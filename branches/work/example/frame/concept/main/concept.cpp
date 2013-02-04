#include "frame/manager.hpp"
#include "system/thread.hpp"

int main(int argc, char *argv[]){
	/*solid::*/Thread::init();
	{
		concept::Manager			m;
		
		solid::aio::SchedulerT		aiosched_1(m);
		solid::aio::SchedulerT		aiosched_2(m);
		solid::ObjectSchedulerT		objsched(m);
		
		solid::ipc::Service			ipcsrvlocal(m, objsched);
		solid::ipc::Service			ipcsrvglobal(m, objsched);
		
		m.localIpc(ipcsrvlocal);
		m.globalIpc(ipcsrvglobal);
		
		concept::alpha::Service		alphasrv(m, objsched);
		concept::beta::Service		betasrv(m, objsched);
		concept::gamma::Service		gammasrv(m, objsched);
		
		m.start();
		
		Configuration				cfg;
		
		int rv;
		
		do{
			cfg.load("concept.cfg");
			
			
			alphasrv.reconfigure(cfg.palpha);
			betasrv.reconfigure(cfg.pbeta);
			gammasrv.reconfigure(cfg.pgamma);
			ipcsrvlocal.reconfigure(cfg.plocalipc);
			ipcsrvglobal.reconfigure(cfg.pglobalipc);
			
			rv = wait_signal();
		}while(rv == OK);
		
		m.stop(false);
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}

