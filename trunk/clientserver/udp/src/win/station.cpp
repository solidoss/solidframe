#include <winsock2.h>
#include <Mswsock.h>

#include "station.h"

namespace udp{
//-----------------------------------------------------------------------------------------
Station *Station::create(){
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) return NULL;
    DWORD	dw = 1;
    if(ioctlsocket(sd, FIONBIO, &dw)){
        closesocket(sd);
        return NULL;
    }
    return hnew Station(sd);
}
//-----------------------------------------------------------------------------------------
Station *Station::create(const InetSocketAddr &_isa){
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) return NULL;
    
    struct sockaddr_in saddr;
    _isa.toSockAddrIn(&saddr);

    if(bind(sd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        return NULL;
    }
    DWORD	dw = 1;
    if(ioctlsocket(sd, FIONBIO, &dw)){
        closesocket(sd);
        return NULL;
    }
    return hnew Station(sd);
}
//-----------------------------------------------------------------------------------------
Station::Station(SOCKET _sd):sd(_sd),rcvsz(0){
}
//-----------------------------------------------------------------------------------------
Station::~Station(){
    closesocket(sd);
}
//-----------------------------------------------------------------------------------------
int Station::recvFrom(char *_pb, unsigned _bl, unsigned _flags){
    if(!_bl) return 0;
    int len = sizeof(rcvaddr);
    int rv = recvfrom(descriptor(), _pb, _bl, 0, (sockaddr*)&rcvaddr, &len);
    if(rv > 0){
        rcvsz = rv;
        rcvd.isa = rcvaddr;
        return 0;
    }
    if(rv == 0) return -1;
    if(rv < 0 && WSAGetLastError() == WSAEWOULDBLOCK){
        rcvd.pb = _pb;
        rcvd.bl = _bl;
        return 1;
    }
    return -1;
}
//-----------------------------------------------------------------------------------------
int Station::sendTo(char *_pb, unsigned _bl, const InetSocketAddr &_isa, unsigned _flags){
    if(!_bl) return 0;
    if(sndq.empty()){
        //struct sockaddr_in saddr;
        _isa.toSockAddrIn(&sndaddr);
        int rv = sendto(descriptor(), _pb, _bl, 0, (const sockaddr*)&sndaddr, sizeof(sndaddr));
        if(rv == _bl) return 0;
        if(rv == 0) return -1;
        if(rv < 0 && WSAGetLastError() == WSAEWOULDBLOCK) return 1;
        hassert_abort(!(rv > 0 && rv < _bl));
        return -1;
    }
    sndq.push(Data(_pb, _bl, _flags));
    sndq.back().isa = _isa;
    return 1;
}
//-----------------------------------------------------------------------------------------
int Station::doRecv(OVERLAPPED *_povp, UINT32 _sz){
    rcvsz = _sz;
    rcvd.isa = rcvaddr;
    return INDONE | OVPDONE;
}
//-----------------------------------------------------------------------------------------
int Station::doSend(OVERLAPPED *_povp, UINT32 _sz){
    hassert_abort(sndq.size());
    sndq.pop();
    while(sndq.size()){
        Data &sd = sndq.front();
        sd.isa.toSockAddrIn(&sndaddr);
        int rv = sendto(descriptor(), sd.pb, sd.bl, 0,(const sockaddr*) &sndaddr, sizeof(sndaddr));
        if(rv == sd.bl){
            if(sd.flags & RAISE_ON_DONE) rv = OUTDONE;
            sndq.pop();
            continue;
        }
        if(rv == 0) return ERRDONE;
        if(rv < 0 && WSAGetLastError() == WSAEWOULDBLOCK){
            if(doPrepareSend(_povp)) return ERRDONE | OVPDONE;
            return rv;
        }
        hassert_abort(!(rv > 0 && rv < sd.bl));
        return ERRDONE;
    }
    return OUTDONE | OVPDONE;
}
//-----------------------------------------------------------------------------------------
//returns -1 on error
int Station::doPrepareRecv(OVERLAPPED *_pov){
    hassert_abort(rcvd.pb);
    WSABUF		*buf = new WSABUF;
    DWORD		sz, flags(0);
    //Tested: It doesn't work sending an empty buffer, it returns imediately
    buf->buf = rcvd.pb;
    buf->len = rcvd.bl;
    addrsz = sizeof(rcvaddr);
    
    int rv = WSARecvFrom(descriptor(), buf, 1, &sz, &flags, (sockaddr*)&rcvaddr, &addrsz, _pov, NULL);
    if(rv != SOCKET_ERROR) return 0;
    if(WSAGetLastError() == WSA_IO_PENDING) return 0;
    ERR(("failed WSARecvFrom %s",getErrorMessage()));
    return -1;
}
//-----------------------------------------------------------------------------------------
int Station::doPrepareSend(OVERLAPPED *_pov){
    //memset(&_povp->ol,0, sizeof(OVERLAPPED));
    //povp->evs = DATAOUT; 
    hassert_abort(sndq.size());
    WSABUF		buf;
    DWORD		sz;
    //Tested: It doesn't work sending an empty buffer, it returns imediately
    buf.buf = sndq.front().pb;
    buf.len = sndq.front().bl;
    int rv = WSASendTo(descriptor(), &buf, 1, &sz, 0, (const sockaddr*)&sndaddr, sizeof(sndaddr), _pov, NULL);
    if(rv != SOCKET_ERROR) return 0;
    if(WSAGetLastError() == WSA_IO_PENDING) return 0;
    ERR(("failed WSARecvFrom %s",getErrorMessage()));
    return -1;
}
//-----------------------------------------------------------------------------------------
}//namespace udp
