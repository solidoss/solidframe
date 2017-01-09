// system/src/sharedbackend.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/sharedbackend.hpp"
#include <deque>
#include <stack>
#include <queue>
#include <mutex>

namespace solid{

struct SharedBackend::Data{
    typedef std::deque<SharedStub>  StubVectorT;
    typedef std::stack<ulong>       UlongStackT;
    typedef std::queue<ulong>       UlongQueueT;
    Data(){
        stubvec.push_back(SharedStub(0));
    }
    std::mutex      mtx;
    StubVectorT     stubvec;
    UlongQueueT     freeque;
};

/*static*/ SharedStub& SharedBackend::emptyStub(){
    static SharedStub ss(0, 1);
    return ss;
}
/*static*/ SharedBackend& SharedBackend::the(){
    static SharedBackend sb;
    return sb;
}
    
SharedStub* SharedBackend::create(void *_pv, SharedStub::DelFncT _cbk){
    SharedStub                      *pss;
    SharedBackend                   &th = the();
    std::unique_lock<std::mutex>    lock(th.d.mtx);
    
    if(th.d.freeque.size()){
        const ulong     pos = th.d.freeque.front();
        th.d.freeque.pop();
        SharedStub      &rss(th.d.stubvec[pos]);
        rss.use = 1;
        rss.ptr = _pv;
        rss.cbk = _cbk;
        pss = &rss;
    }else{
        const ulong     pos = th.d.stubvec.size();
        th.d.stubvec.push_back(SharedStub(pos));
        SharedStub      &rss(th.d.stubvec[pos]);
        rss.ptr = _pv;
        rss.use = 1;
        rss.cbk = _cbk;
        pss = &rss;
    }
    return pss;
}

void SharedBackend::doRelease(SharedStub &_rss){
    (*_rss.cbk)(_rss.ptr);
    std::unique_lock<std::mutex>    lock(d.mtx);
    d.freeque.push(_rss.idx);
}

SharedBackend::SharedBackend():d(*(new Data)){
}
SharedBackend::~SharedBackend(){
    //delete &d;
}

}//namespace solid

