#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <cstdio>

int main(){
    int fd = epoll_create(1000);
    printf("fd = %d", fd);
    int rv = epoll_pwait2(fd, nullptr, 0, nullptr, nullptr);
    printf("rv = %d", rv);
    return 0;
}
