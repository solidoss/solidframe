#ifndef UDPTALKERSELECTOR_H
#define UDPTALKERSELECTOR_H

#include <vector>
#include <stack>
#include <queue>
#include "common/h.h"

namespace udp{

class Talker;

class TalkerSelector{
public:
    TalkerSelector();
    ~TalkerSelector();
    int init();
    void run();
    void push(Talker *_ptkr);
    void signal(UINT32 _objid = 0);
    int  empty()const       {return !sz;}
private:
    enum {EXIT_LOOP = 1, FULL_SCAN = 2};
    struct SelStation{
        SelStation(Talker *_ptkr = NULL, int _state = 1):ptkr(_ptkr),state(_state), ovpcnt(0){}
        Talker  *ptkr;
        UINT32  timepos;
        short   state;
        short   ovpcnt;
    };
    typedef std::stack<UINT32>			SelStkTp;
    typedef std::deque<SelStation>		SelVecTp;
    class  OvpDeque;
    struct OverlappedPlus;
    typedef std::stack<OverlappedPlus*, 
                        std::vector<OverlappedPlus*> >		OvpStkTp;
    typedef Queue<UINT32>									SelQueueTp;
private:
    void releaseOverlapped(OverlappedPlus *&_povp);
	OverlappedPlus* createOverlapped();
	void releaseStation(UINT32 idx);
	UINT32 createStation();
	void doFullScan();
	int doIo(OverlappedPlus *povp, int, UINT32);
	void doExec(UINT32, UINT32);
private:
    int             sz;
    SelVecTp        sv;
    SelStkTp        ss;
    SelQueueTp      sq;
    UINT64          btimepos;//begin time pos
    UINT32          ntimepos;//next timepos == next timeout
    UINT32          ctimepos;//current time pos
    HANDLE          hioc;
    OverlappedPlus	*psigovp;
    OvpDeque        &ovpd;
    OvpStkTp        ovps;
};

}//namespace udp

#endif