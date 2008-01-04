#include "common/halloc.h"
#include "talkerselector.h"


#include "common/utc.h"

#include "../talker.h"
#include "station.h"

namespace udp{

enum {MAXPOLLTOUT = ((0xffffffff >> 1) / 1000)};

struct TalkerSelector::OverlappedPlus{
    OverlappedPlus(UINT32 _idx = 0, UINT32 _state = 0):idx(_idx), state(_state){}
    OVERLAPPED  ov;
    UINT32      idx;
    UINT32      state;
};

struct TalkerSelector::OvpDeque: std::deque<OverlappedPlus>{};

TalkerSelector::TalkerSelector():ovpd(*(hnew OvpDeque)), sz(0){
    psigovp = createOverlapped();
    createStation();// the 0 index is reserved
}
TalkerSelector::~TalkerSelector(){
    OvpDeque *povpd = &ovpd;
    hdel(povpd);
}
int TalkerSelector::init(){
    psigovp->idx = 0;
	memset(&psigovp->ov,0, sizeof(OVERLAPPED));
    hioc = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if(hioc == NULL){
        ERR(("Unable to acreate completion port"));
        return -1;
    }

    btimepos = UTC64::nowSec();
    ctimepos = 0;
    ntimepos = 0xffffffff;
    return 0;
}
void TalkerSelector::run(){
    int             state = 0;
    int             tout;
    ULONG_PTR		*phandlekey;
	OVERLAPPED		*pov;
    OverlappedPlus  *povp;
	DWORD			bx;
    do{
        state = 0;
        if(ntimepos != 0xffffffff) tout = ntimepos - ctimepos;
        else tout = -1;
        if(sq.size()) tout = 0;
        if(tout > MAXPOLLTOUT) tout = MAXPOLLTOUT;
        INFO(("before getqueued... tout = %d qsz = %u sz = %u ntimepos = %u ctimepos = %u", tout, sq.size(), sv.size(), ntimepos, ctimepos));
        int rv = GetQueuedCompletionStatus(hioc, &bx, (PULONG_PTR)&phandlekey, &pov, tout);
        ctimepos = UTC64::nowSec() - btimepos;
        if(pov){
            if(pov != &psigovp->ov){
                povp = CONTAINING_RECORD(pov,OverlappedPlus, ov);
                if(sv[povp->idx].ptkr){
                    UINT32 idx = povp->idx;
                    if(rv = doIo(povp, rv, bx)){
                        doExec(idx, rv);
                    }
                }else{//a deleted talker, recicle
                    if(!(--sv[povp->idx].ovpcnt)) releaseStation(povp->idx);
                    releaseOverlapped(povp);
                }
            }else{
                if(bx){//signaled a Talker
                    if(bx < sv.size() && sv[bx].ptkr && sv[bx].ptkr->signaled(Talker::S_RAISE) && !sv[bx].state){
                        //sq.push(&sv[bx]); sv[bx].state = 1;
                        doExec(bx, 0);
                    }
                }else{//signaled to exit the loop
                    state |= EXIT_LOOP;
                }
            }
        }
        if(ctimepos >= ntimepos){
            doFullScan();
        }
        {
            int sz = sq.size(); if(sz > 4) sz = 4;
            while(sz){
                --sz;
                rv = 0;
                UINT32 idx = sq.front(); sq.pop(); 
                if(sv[idx].state == 2) rv = Station::ERRDONE;
                sv[idx].state = 0;
                doExec(idx, rv);
            }
        }
    }while(sz && !(state & EXIT_LOOP));    
}

void TalkerSelector::push(Talker *_ptkr){
    UINT32  idx = createStation();
    SelStation &rs = sv[idx];
    rs.ptkr = _ptkr;
    rs.state = 1;//in queue
    rs.timepos = 0xffffffff;
    _ptkr->setThread(0,idx);
    if(CreateIoCompletionPort((HANDLE)_ptkr->station().descriptor() , hioc, (ULONG_PTR)0, 0) == NULL){
		rs.state = 2;//in queue but with error
	}
	sq.push(idx);
	++sz;
}

void TalkerSelector::doFullScan(){
}
int TalkerSelector::doIo(OverlappedPlus *povp, int rv, UINT32 _sz){
    if(!rv) return Station::ERRDONE;
    SelStation &rs(sv[povp->idx]);
    switch(povp->state){
        case Station::INTOUT:  rv = rs.ptkr->station().doRecv(&povp->ov, _sz);break;
        case Station::OUTTOUT: rv = rs.ptkr->station().doSend(&povp->ov, _sz);break;
    }
    if(rv & Station::OVPDONE){--rs.ovpcnt; releaseOverlapped(povp);}
    return rv & Station::SIGMSK;
}
void TalkerSelector::doExec(UINT32 _idx, UINT32 _sig){
    UINT32 tout;
    SelStation &rs(sv[_idx]);
    switch(rs.ptkr->execute(_sig, tout)){
        case Talker::BAD:
            if(rs.ovpcnt){
                CancelIo((HANDLE)rs.ptkr->station().descriptor());
                hdel(rs.ptkr);
            }else{
                hdel(rs.ptkr);
                releaseStation(_idx);
            }
            --sz;
            break;
        case Talker::OK:
            sq.push(_idx); rs.state = 1;
            break;
        case Talker::TOUT:
            UINT32 iorq = rs.ptkr->station().ioRequest();
            if(iorq & Station::INTOUT){
                OverlappedPlus *povp = createOverlapped();
                memset(&povp->ov,0, sizeof(OVERLAPPED));
                povp->state = Station::INTOUT;
                povp->idx = _idx;
                if(rs.ptkr->station().doPrepareRecv(&povp->ov)){
                    releaseOverlapped(povp);
                }else ++rs.ovpcnt;
            }
            if(iorq & Station::OUTTOUT){
                OverlappedPlus *povp = createOverlapped();
                memset(&povp->ov,0, sizeof(OVERLAPPED));
                povp->state = Station::OUTTOUT;
                povp->idx = _idx;
                if(rs.ptkr->station().doPrepareSend(&povp->ov)){
                    releaseOverlapped(povp);
                }else ++rs.ovpcnt;
            }
            break;
    }
}

void TalkerSelector::signal(UINT32 _objid){
    PostQueuedCompletionStatus(hioc, _objid, NULL, &psigovp->ov);
}
void TalkerSelector::releaseOverlapped(OverlappedPlus *&_povp){
    _povp->idx = 0;
    ovps.push(_povp);
    _povp = NULL;
}
TalkerSelector::OverlappedPlus* TalkerSelector::createOverlapped(){
    if(ovps.size()){
        OverlappedPlus *povp = ovps.top(); ovps.pop();
        return povp;
    }
    ovpd.push_back(OverlappedPlus());
    return &ovpd.back();
}

void TalkerSelector::releaseStation(UINT32 _idx){
    ss.push(_idx);
}

UINT32 TalkerSelector::createStation(){
    if(ss.size()){
        UINT32 id = ss.top(); ss.pop();
        return id;
    }
    sv.push_back(SelStation());
    return sv.size() - 1;
}

}//namespace udp
