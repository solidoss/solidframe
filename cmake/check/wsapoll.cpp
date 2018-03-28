#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

#define WS_VER          0x0202
#define DEFAULT_WAIT    30000

int main(){
    WSADATA             wsd;
    INT nerr = 0;
    INT ret = 0;
    nerr = WSAStartup(WS_VER, &wsd);
    WSAPOLLFD           fdarray = {0};
    ret = WSAPoll(&fdarray, 1, DEFAULT_WAIT);
    return 0;
}