#include "frame/manager.hpp"
#include "system/thread.hpp"

int main(int argc, char *argv[]){
	/*solid::*/Thread::init();
	{
		concept::Manager			m;
		
		solid::aio::SchedulerT		aiosched_1(m);
		solid::aio::SchedulerT		aiosched_2(m);
		solid::ObjectSchedulerT		objsched(m);
		
		solid::ipc::Service			ipcsvclocal(m);
		solid::ipc::Service			ipcsvcglobal(m);
		
		concept::alpha::Service		alphasvc(m);
		concept::beta::Service		betasvc(m);
		concept::gamma::Service		gammasvc(m);

		m.registerService(ipcsvclocal);
		m.registerService(ipcsvcglobal);
		m.registerService(alphasvc);
		m.registerService(betasvc);
		m.registerService(gammasvc);
		
		m.registerObject(alphasvc.pointer());
		m.registerObject(betasvc.pointer());
		m.registerObject(gammasvc.pointer());
		
		objsched.push(alphasvc.pointer());
		objsched.push(betasvc.pointer());
		objsched.push(gammasvc.pointer());
		
		m.localIpc(ipcsrvlocal);
		m.globalIpc(ipcsrvglobal);

		Configuration				cfg;
		
		int							rv;
		
		do{
			cfg.load("concept.cfg");
			
			
			alphasvc.reconfigure(cfg.palpha);
			betasvc.reconfigure(cfg.pbeta);
			gammasvc.reconfigure(cfg.pgamma);
			ipcsvclocal.reconfigure(cfg.plocalipc);
			ipcsvcglobal.reconfigure(cfg.pglobalipc);
			
			rv = wait_signal();
		}while(rv == OK);
		
		m.stop(false);
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}

