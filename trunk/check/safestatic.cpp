//TODO: usefull comment
#include <cstdlib>
#include <cassert>
using namespace std;

void ssleep();
void sssleep();

static int counter = 0;

struct A{
	A(int _v=0):v(_v){
		ssleep();
		v = -1;
	}
	int v;
};

int getV(){
	static A a;
	return a.v;
}

#ifdef __WIN32
void init(){
	//on windows allways a problem with the static
	assert(false);
}
#else

#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mut;

void init(){
	counter = 0;
// 	pthread_mutexattr_t att;
// 	pthread_mutexattr_init(&att);
// 	pthread_mutexattr_settype(&att, (int)ERRORCHECK);
	pthread_mutex_init(&mut,NULL);
	//pthread_mutexattr_destroy(&att);
}

void ssleep(){
	pthread_mutex_lock(&mut);
	assert(!counter);
	counter = 1;
	pthread_mutex_unlock(&mut);
	sleep(1);
}

void sssleep(){
	sleep(2);
}

void* th_run(void *pv){
	assert(getV());
}

void create_thread(){
	pthread_t th;
	pthread_create(&th,NULL,&th_run,NULL);
}
#endif

int main(){
	init();
	create_thread();
	assert(getV());
	sssleep();
	return 0;
}
