#include "system/timespec.hpp"
#include "system/condition.hpp"
#include "system/mutex.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"

#include <string>
#include <iostream>

using namespace std;
using namespace solid;


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
#ifdef ON_WINDOWS
	WSADATA	wsaData;
    int		err;
	WORD	wVersionRequested;
/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif

#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");

	/*Debug::the().initStdErr(
		false,
		&dbgout
	);*/
	Debug::the().initSocket("192.168.201.100", "1114", false, &dbgout);
	//Debug::the().initFile("test_wait",false);
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Debug::the().moduleNames(dbgout);
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
	Thread::waitAll();
	return 0;
}
