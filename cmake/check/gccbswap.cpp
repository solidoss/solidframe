#include <stdint.h>
int main(){
    int32_t v1 =  __builtin_bswap32 (0xff00ff00);
    int64_t v2 =  __builtin_bswap64 (0xff00ff00ff00ff00ull);
    return v1+v2;
}
