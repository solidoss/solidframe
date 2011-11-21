#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <cstdio>

int main(){
	int kq = kqueue();
	printf("kq = %d", kq);
	return 0;
}
