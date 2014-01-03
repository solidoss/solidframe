#include <iostream>
#include "system/thread.hpp"
#include "sharedobjectmanager.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"

using namespace std;
using namespace solid;

namespace{
	Mutex		mtx;
	Condition	cnd;
	size_t		wkrcnt = 0;
}

struct Worker: Thread{
	SharedObjectManager	&rsom;
	const size_t		thridx;
	const size_t		notify_count;
	const size_t		insert_count;
	
	Worker(
		SharedObjectManager	&_rsom,
		size_t _thridx,
		size_t _notify_count,
		size_t _insert_count
	):rsom(_rsom), thridx(_thridx), notify_count(_notify_count), insert_count(_insert_count){}
	bool start(){
		return Thread::start(true);
	}
	void run();
};

int main(int argc, char *argv[]){
	const size_t all_flg_repeat_cnt = 100;
	const size_t all_val_repeat_cnt = 100;
	const size_t producer_cnt = 10;
	const size_t notify_cnt = 1000 * 1000 * 10;
	const size_t insert_cnt = 1000;
	
	SharedObjectManager som;
	
	som.start();
	{
		Locker<Mutex>	lock(mtx);
		for(size_t i = 0; i < producer_cnt; ++i){
			Worker *pw = new Worker(som, i, notify_cnt, insert_cnt);
			if(pw->start()){
				++wkrcnt;
			}
		}
	}
	
	for(size_t i = 0; i < all_flg_repeat_cnt; ++i){
		som.notifyAll(SharedObjectManager::Flag1);
	}
	
	for(size_t i = 0; i < all_val_repeat_cnt; ++i){
		som.notifyAll(SharedObjectManager::Flag2, i);
	}
	
	{
		Locker<Mutex>	lock(mtx);
		while(wkrcnt != 0){
			cnd.wait(lock);
		}
	}
	som.stop(cout);
	Thread::waitAll();
	return 0;
}

void Worker::run(){
#if 0
	const size_t	step = notify_count / insert_count;
	for(size_t i = 0; i < notify_count; ++i){
		if((i % step) == 0){
			rsom.insert(thridx ^ i);
		}
		if((i % 2) == 0){
			rsom.notify(i, SharedObjectManager::Flag3);
		}else{
			rsom.notify(i, SharedObjectManager::Flag4, i ^ thridx);
		}
	}
#endif
#if 1
	size_t j = 0;
	for(size_t i = 0; i < notify_count; ++i){
		if(j < insert_count){
			rsom.insert(thridx ^ i);
			++j;
		}
		if((i % 2) == 0){
			rsom.notify(i, SharedObjectManager::Flag3);
		}else{
			rsom.notify(i, SharedObjectManager::Flag4, i ^ thridx);
		}
	}
#endif
	rsom.notifyAll(SharedObjectManager::Flag1);
	rsom.notifyAll(SharedObjectManager::Flag2, thridx);
	{
		Locker<Mutex>	lock(mtx);
		--wkrcnt;
		cnd.signal();
	}
}