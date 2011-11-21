#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

int main(){
	int kq = kqueue();
	printf("kq = %d\n", kq);
	return 0;
}
