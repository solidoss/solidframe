#include "system/mutex.hpp"
#include "system/condition.hpp"
#include <signal.h>
#include <cstring>
#include <iostream>

using namespace std;
using namespace solid;

struct Waiter{
	Waiter():v(0){}
	Condition	cnd;
	Mutex		mtx;
	int 		v;
	bool wait(){
		Locker<Mutex> lock(mtx);
		while(!v){
			cout<<"before wait"<<endl;
			cnd.wait(lock);
			cout<<"after wait"<<endl;
		}
		if(v > 0) return true;
		return false;
	}
} w;

void sig_cbk(int _sig){
	w.v = -1;
	cout<<"signaled "<<_sig<<endl;
	w.cnd.signal();
}

int main(){
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_cbk; 
	sigaction(SIGTERM, &sa, NULL);
	
	while(w.wait()){
		w.v = 0;
	}
	
	return 0;
}


