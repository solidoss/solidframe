#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "utility/queue.hpp"
#include <iostream>

using namespace solid;
using namespace std;





class Consumer: private Thread{
public:
	Consumer(const uint64 _waitcnt):waitcnt(_waitcnt){}
	void start();
private:
	void run();
private:
	const uint64	waitcnt;
	Mutex			mtx;
	Condition		cnd;
};

int main(int argc, char * argv[]){
	
	if(argc != 3){
		cout<<"Usage:\n\t$ example_allocatetest 3 1000\n\t\tWhere 3 is the number of producers and 1000 is the number of iterations\n";
		return 0;
	}
	
	uint64			prdcnt = atoi(argv[1]);
	uint64			rptcnt = atoi(argv[2]);
	
	Consumer	*pcons = new Consumer(prdcnt * rptcnt);
	pcons->start();
	
	for(int i = 0; i < prdcnt; ++i){
		Producer *prod = new Producer(*pcons, rptcnt);
		prod->start();
	}
	
	
	Thread::waitAll();
	return 0;
}
