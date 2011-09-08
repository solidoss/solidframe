//TODO: usefull comment
#include <cstdlib>
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

#ifdef _WIN32
#include <Windows.h>

static CRITICAL_SECTION cs;

void init(){
	//on windows allways a problem with the static
	//assert(false);
	InitializeCriticalSection(&cs);
}
DWORD th_run(void *_pv){
	int v = getV();
	//assert(v);
	if(v == 0){
		exit(-3);
	}
	return 0;
}
void create_thread(){
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&th_run, NULL, 0, NULL);
}

void ssleep(){
	EnterCriticalSection(&cs);
	//assert(!counter);
	if(counter != 0){
		exit(-2);
	}
	counter = 1;
	LeaveCriticalSection(&cs);
	Sleep(1000);
}

void sssleep(){
	Sleep(2000);
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
	//assert(!counter);
	if(counter != 0){
		exit(-3);
	}
	counter = 1;
	pthread_mutex_unlock(&mut);
	sleep(1);
}

void sssleep(){
	sleep(2);
}

void* th_run(void *pv){
	//assert(getV());
	int v = getV();
	//assert(v);
	if(v == 0){
		exit(-2);
	}
	return NULL;
}

void create_thread(){
	pthread_t th;
	pthread_create(&th,NULL,&th_run,NULL);
}
#endif

int main(){
	init();
	create_thread();
	int v = getV();
	//assert(v);
	if(v == 0){
		return -1;
	}
	sssleep();
	return 0;
}