#include <pthread.h>

int main(){
    pthread_spinlock_t spl;
    auto rv = pthread_spin_init(&spl, PTHREAD_PROCESS_PRIVATE);
    if(rv != 0) return -1;
    return 0;
}