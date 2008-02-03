#include "writer.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "system/common.h"
using namespace std;

bool isquotedspecial(uint8 _c){
	if(_c == '\\') return false;
	if(_c == '\"') return false;
	return isgraph(_c);
}

void Writer::reinit(int _sd){
	sd = _sd;
	bpos = bbeg;
}

int Writer::flush(){
    uint32 len = bpos-bbeg;
    if(len){
        wrerr = write(sd,bbeg,len);
        count += wrerr;
        bpos=bbeg;
    }
	return OK;
}

void Writer::put(const char *_s,uint32 _sz){
    uint32 rem = bend - bpos;
    if(_sz < rem){
        std::memcpy(bpos,_s,_sz);
        bpos+=_sz;
        return;
    }
    std::memcpy(bpos,_s,rem);
    _s += rem;
    _sz -= rem;
    bpos += rem;
    flush();
    if(_sz == 0) return;
    rem = _sz % BUFFLEN;
    _sz -= rem;
    if(_sz){
        if(wrerr >= 0){
            wrerr = write(sd,_s,_sz);
            count += wrerr;
        }
    }
    std::memcpy(bpos,_s+_sz,rem);
    bpos+=rem;
}
void Writer::put(uint32 _v){
    if(!_v){
        put('0');
    }else{
        char tmp[12];
        ushort pos=11;
        while(_v){
            *(tmp + pos)='0' + _v % 10;
            _v /= 10;
            --pos;
        }
        ++pos;
        put(tmp+pos,12 - pos);
    }
}


