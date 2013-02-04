#include "frame/manager.hpp"
#include "system/thread.hpp"

int main(int argc, char *argv[]){
	/*solid::*/Thread::init();
	{
		solid::Manager			m;
		solid::aio::SchedulerT	aiosched(m);
		solid::ObjectSchedulerT	objsched(m);
		solid::Service			srv(m, objsched);
		
		m.start();
		srv.start();
		{
			Listener::SharedPointerT objptr(new Listener(srv, aiosched,/*sd, pctx*/));
			aiosched.schedule(objptr);
			
			//the Object will register itself on constructor and unregister itself from the service on destructor
			//so the id is guaranteed - its unique part is computed by manager
			m.notify(m.id(*objptr), solid::RaiseSignal);
			SupperMessage::SharedPointerT	msgptr(new SupperMessage());
			m.notify(m.id(*objptr), msgptr);
		}
		
		wait_sig_term();
		m.stop(false);
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}

