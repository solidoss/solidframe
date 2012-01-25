#include "system/timespec.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include <string>
#include <iostream>
using namespace std;



struct MyThread: Thread{
    MyThread(
		Mutex &_rm,
		Condition &_rc,
		int &_rv
	):rmtx(_rm), rcnd(_rc), rval(_rv){}
	void run();
private:
	Mutex		&rmtx;
	Condition	&rcnd;
	int			&rval;
};

void MyThread::run(){
	idbg("thread started");
	Locker<Mutex>	lock(rmtx);
	TimeSpec until(TimeSpec::createRealTime());
	until += 1000;
	idbg("lock aquired - wait condition");
	int cnt(15);
	while(cnt-- && rval != 0){
		int rv = rcnd.wait(lock, until);
		idbg("waiting "<<rv);
		until += 1110;
	}
	idbg("done wait "<<rval);
	rval = 2;
	idbg("done thread");
	rcnd.signal();
}

int main(int argc, char *argv[]){
	Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Dbg::instance().levelMask("view");
	Dbg::instance().moduleMask("any");

	Dbg::instance().initStdErr(
		false,
		&dbgout
	);
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Dbg::instance().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif

	Mutex		mtx;
	Condition	cnd;
	int			val(1);
	MyThread	*thr = new MyThread(mtx, cnd, val);
	idbg("before thread start");
	{
		Locker<Mutex>	lock(mtx);
		thr->start();
		Thread::sleep(100);
	}
	idbg("sleep 10 seconds");
	Thread::sleep(1000*10);
	idbg("done sleeping - set value to 0");
	{
		Locker<Mutex>	lock(mtx);
		val = 0;
		cnd.signal();
	}
	{
		Locker<Mutex>	lock(mtx);
		while(val != 2){
			cnd.wait(lock);
		}
	}
	return 0;
}
