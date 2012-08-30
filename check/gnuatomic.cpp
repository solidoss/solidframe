#include <ext/atomicity.h>

int main(int argc, char *argv[]){
	int  r;
	__gnu_cxx:: __atomic_add_dispatch(&r, 1);
	__gnu_cxx::__exchange_and_add_dispatch(&r, -1);
	return 0;
}