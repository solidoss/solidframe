// solid/frame/src/execscheduler.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <vector>
#include "solid/frame/schedulerbase.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/service.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include "solid/system/cassert.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/algorithm.hpp"
#include <atomic>

#include <memory>

using namespace std;

namespace solid{
namespace frame{

namespace{

class ErrorCategory: public ErrorCategoryT{
public:
    enum{
        AlreadyE = 1,
        WorkerE,
        RunningE,
        ReactorE
    };

    ErrorCategory(){}
private:
    const char*   name() const noexcept (true){
        return "solid::frame::Scheduler";
    }

    std::string    message(int _ev) const{
        switch(_ev){
            case AlreadyE:
                return "Already started";
            case WorkerE:
                return "Failed to start worker";
            case RunningE:
                return "Scheduler not running";
            case ReactorE:
                return "Reactor failure";
            default:
                return "Unknown";
        }
    }
};

const ErrorCategory     ec;

inline ErrorConditionT error_already(){
    return ErrorConditionT(ErrorCategory::AlreadyE, ec);
}
inline ErrorConditionT error_worker(){
    return ErrorConditionT(ErrorCategory::WorkerE, ec);
}

inline ErrorConditionT error_running(){
    return ErrorConditionT(ErrorCategory::RunningE, ec);
}

// inline ErrorConditionT error_reactor(){
//  return ErrorConditionT(ErrorCategory::ReactorE, ec);
// }

}//namespace

typedef Queue<UniqueId>                 UidQueueT;
typedef Stack<UniqueId>                 UidStackT;
typedef std::atomic<size_t>     AtomicSizeT;

struct ReactorStub{
    ReactorStub(ReactorBase *_preactor = nullptr):preactor(_preactor){}

    ReactorStub(ReactorStub && _urs):thr(std::move(_urs.thr)), preactor(_urs.preactor){
        _urs.preactor = nullptr;
    }

    bool isActive()const{
        static const std::thread empty_thread{};
        return thr.get_id() != empty_thread.get_id();
    }

    void clear(){
        preactor = nullptr;
    }

    thread          thr;
    ReactorBase     *preactor;
};

typedef std::vector<ReactorStub>            ReactorVectorT;

enum Statuses{
    StatusStoppedE = 0,
    StatusStartingWaitE,
    StatusStartingErrorE,
    StatusRunningE,
    StatusStoppingE,
    StatusStoppingWaitE
};

typedef std::atomic<Statuses>           AtomicStatuesT;

struct SchedulerBase::Data{
    Data():crtreactoridx(0), reactorcnt(0), stopwaitcnt(0), status(StatusStoppedE), usecnt(0){
    }

    size_t                      crtreactoridx;
    size_t                      reactorcnt;
    size_t                      stopwaitcnt;
    AtomicStatuesT              status;
    AtomicSizeT                 usecnt;
    ThreadEnterFunctionT        threnfnc;
    ThreadExitFunctionT         threxfnc;

    ReactorVectorT              reactorvec;
    mutex                       mtx;
    condition_variable          cnd;
};


SchedulerBase::SchedulerBase(
):d(*(new Data)){
}

SchedulerBase::~SchedulerBase(){
    doStop(true);
    delete &d;
}


bool dummy_thread_enter(){
    return true;
}
void dummy_thread_exit(){
}

ErrorConditionT SchedulerBase::doStart(
    CreateWorkerF _pf,
    ThreadEnterFunctionT &_renf, ThreadExitFunctionT &_rexf,
    size_t _reactorcnt
){
    if(_reactorcnt == 0){
        _reactorcnt = thread::hardware_concurrency();
    }
    bool start_err = false;

    {
        unique_lock<mutex>  lock(d.mtx);

        if(d.status == StatusRunningE){
            return error_already();
        }else if(d.status != StatusStoppedE || d.stopwaitcnt){
            do{
                d.cnd.wait(lock);
            }while(d.status != StatusStoppedE || d.stopwaitcnt);
        }

        d.reactorvec.resize(_reactorcnt);


        if(not FUNCTION_EMPTY(_renf)){
            FUNCTION_CLEAR(d.threnfnc);
            std::swap(d.threnfnc, _renf);
        }else{
            d.threnfnc = dummy_thread_enter;
        }

        if(not FUNCTION_EMPTY(_rexf)){
            FUNCTION_CLEAR(d.threxfnc);
            std::swap(d.threxfnc, _rexf);
        }else{
            d.threxfnc = dummy_thread_exit;
        }

        for(size_t i = 0; i < _reactorcnt; ++i){
            ReactorStub &rrs = d.reactorvec[i];
            //rrs.thrptr.reset((*_pf)(*this, i));


            if(not (*_pf)(*this, i, rrs.thr)){
                start_err = true;
                break;
            }
        }


        if(!start_err){
            d.status = StatusStartingWaitE;

            do{
                d.cnd.wait(lock);
            }while(d.status == StatusStartingWaitE && d.reactorcnt != d.reactorvec.size());

            if(d.status == StatusStartingErrorE){
                d.status = StatusStoppingWaitE;
                start_err = true;
            }
        }

        if(start_err){
            for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
                if(it->preactor){
                    it->preactor->stop();
                }
            }
        }else{
            d.status = StatusRunningE;
            return ErrorConditionT();
        }
    }

    for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
        if(it->isActive()){
            it->thr.join();
            it->clear();
        }
    }

    unique_lock<mutex>  lock(d.mtx);
    d.status = StatusStoppedE;
    d.cnd.notify_all();
    return error_worker();
}

void SchedulerBase::doStop(bool _wait/* = true*/){
    {
        unique_lock<mutex>  lock(d.mtx);

        if(d.status == StatusStartingWaitE || d.status == StatusStartingErrorE){
            do{
                d.cnd.wait(lock);
            }while(d.status == StatusStartingWaitE || d.status == StatusStartingErrorE);
        }

        if(d.status == StatusRunningE){
            d.status = _wait ? StatusStoppingWaitE : StatusStoppingE;
            for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
                if(it->preactor){
                    it->preactor->stop();
                }
            }
        }else if(d.status == StatusStoppingE){
            d.status = _wait ? StatusStoppingWaitE : StatusStoppingE;
        }else if(d.status == StatusStoppingWaitE){
            if(_wait){
                ++d.stopwaitcnt;
                do{
                    d.cnd.wait(lock);
                }while(d.status != StatusStoppedE);
                --d.stopwaitcnt;
                d.cnd.notify_one();
            }
            return;
        }else if(d.status == StatusStoppedE){
            return;
        }
    }

    if(_wait){
        while(d.usecnt){
            this_thread::yield();
        }
        for(auto it = d.reactorvec.begin(); it != d.reactorvec.end(); ++it){
            if(it->isActive()){
                it->thr.join();
                it->clear();
            }
        }
        unique_lock<mutex>  lock(d.mtx);
        d.status = StatusStoppedE;
        d.cnd.notify_all();
    }
}

ObjectIdT SchedulerBase::doStartObject(ObjectBase &_robj, Service &_rsvc, ScheduleFunctionT &_rfct, ErrorConditionT &_rerr){
    ++d.usecnt;
    ObjectIdT   rv;
    if(d.status == StatusRunningE){
        ReactorStub     &rrs = d.reactorvec[doComputeScheduleReactorIndex()];

        rv = _rsvc.registerObject(_robj, *rrs.preactor, _rfct, _rerr);
    }else{
        _rerr = error_running();
    }
    --d.usecnt;
    return rv;
}

bool less_cmp(ReactorStub const &_rrs1, ReactorStub const &_rrs2){
    return _rrs1.preactor->load() < _rrs2.preactor->load();
}

size_t SchedulerBase::doComputeScheduleReactorIndex(){
    switch(d.reactorvec.size()){
        case 1:
            return 0;
        case 2:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<2>());
        }
        case 3:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<3>());
        }
        case 4:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<4>());
        }
        case 5:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<5>());
        }
        case 6:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<6>());
        }
        case 7:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<7>());
        }
        case 8:{
            return find_cmp(d.reactorvec.begin(), less_cmp, SizeToType<8>());
        }
        default:
            break;
    }

    const size_t    cwi = d.crtreactoridx;
    d.crtreactoridx = (cwi + 1) % d.reactorvec.size();
    return cwi;
}

bool SchedulerBase::prepareThread(const size_t _idx, ReactorBase &_rreactor, const bool _success){
    const bool      thrensuccess = d.threnfnc();
    {
        unique_lock<mutex>  lock(d.mtx);
        ReactorStub     &rrs = d.reactorvec[_idx];

        if(_success && d.status == StatusStartingWaitE && thrensuccess){
            rrs.preactor = &_rreactor;
            ++d.reactorcnt;
            d.cnd.notify_all();
            return true;
        }

        d.status = StatusStartingErrorE;
        d.cnd.notify_one();
    }

    d.threxfnc();

    return false;
}

void SchedulerBase::unprepareThread(const size_t _idx, ReactorBase &_rreactor){
    {
        unique_lock<mutex>  lock(d.mtx);
        ReactorStub     &rrs = d.reactorvec[_idx];
        rrs.preactor = nullptr;
        --d.reactorcnt;
        d.cnd.notify_one();
    }
    d.threxfnc();
}


}//namespace frame
}//namespace solid
