#ifndef UDPSTATION_H
#define UDPSTATION_H

#include <queue>
#include "socket/inetsocket.h"

//struct OVERLAPPED;

namespace udp{
class TalkerSelector;
class Station{
public:
    enum Consts{
        RAISE_ON_DONE = 2, 
        ERRDONE = 1, 
        TIMEOUT = 2, 
        INDONE = 4, 
        OUTDONE = 8, 
        SIGMSK = ERRDONE | TIMEOUT | INDONE | OUTDONE,
        OVPDONE = 16
    };
    ///An udp client station
    static Station *create();
    ///An udp server station bound to a local port
    static Station *create(const InetSocketAddr &_isa);
    ~Station();
    int recvFrom(char *_pb, unsigned _bl, unsigned _flags = 0);
    int sendTo(char *_pb, unsigned _bl, const InetSocketAddr &_isa, unsigned _flags = 0);
    unsigned recvSize()const;
    const InetSocketAddr &recvAddr() const;
private:
    friend class TalkerSelector;
    struct Data{
        Data(char *_pb = NULL, unsigned _bl = 0, unsigned _flags = 0):pb(_pb), bl(_bl),flags(0){}
        Data(const InetSocketAddr &_isa, char *_pb = NULL, unsigned _bl = 0, unsigned _flags = 0):pb(_pb), 
                                                                                                bl(_bl),
                                                                                                flags(0),
                                                                                                isa(_isa){}
        char            *pb;
        unsigned        bl;
        unsigned        flags;
        InetSocketAddr  isa;
    };
    typedef Queue<Data,4>    DataQueueTp;
    enum {INTOUT = 1, OUTTOUT = 2};
    Station(SOCKET sd);
    UINT32 ioRequest()const;
    int doSend();
    //int doRecv();
    int doSend(OVERLAPPED *_povp, UINT32 _sz);
    int doRecv(OVERLAPPED *_povp, UINT32 _sz);
    int doPrepareRecv(OVERLAPPED *_povp);
    int doPrepareSend(OVERLAPPED *_povp);
    SOCKET descriptor() const;
private:
    SOCKET          sd;
    unsigned        rcvsz;
    Data            rcvd;
    DataQueueTp     sndq;
    sockaddr_in     rcvaddr;
    sockaddr_in     sndaddr;
    int             addrsz;
};

inline unsigned Station::recvSize()              const{return rcvsz;}
inline const InetSocketAddr &Station::recvAddr() const{return rcvd.isa;}
inline SOCKET Station::descriptor()              const{return sd;}
inline UINT32 Station::ioRequest()const{
    UINT32 rv = rcvd.pb ? INTOUT : 0;
    if(sndq.size()) rv |= OUTTOUT;
    return rv;
}
}//namespace udp
#endif
