// solid/utility/workpool.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_WORKPOOL_HPP
#define UTILITY_WORKPOOL_HPP

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#include "solid/utility/common.hpp"
#include "solid/utility/queue.hpp"

namespace solid{

//! Base class for every workpool workers
struct WorkerBase{
    uint32_t    wkrid;
};

//! Base class for workpool
struct WorkPoolBase{
    enum State{
        Stopped = 0,
        Stopping,
        Running
    };
    State state()const{
        return st;
    }
    void state(State _s){
        st = _s;
    }
    bool isRunning()const{
        return st == Running;
    }
    bool isStopping()const{
        return st == Stopping;
    }
protected:
    WorkPoolBase():st(Stopped), wkrcnt(0) {}
    
    State                       st;
    int                         wkrcnt;
    std::condition_variable     thrcnd;
    std::condition_variable     sigcnd;
    std::mutex                  mtx;
};

//! A controller structure for WorkPool
/*!
 * The WorkPool will call methods of this structure.
 * Inherit and implement the needed methods.
 * No need for virtualization as the Controller is
 * a template parameter for WorkPool.
 */
struct WorkPoolControllerBase{
    bool prepareWorker(WorkerBase &_rw){
        return true;
    }
    void unprepareWorker(WorkerBase &_rw){
    }
    void onPush(WorkPoolBase &){
    }
    void onMultiPush(WorkPoolBase &, size_t _cnd){
    }
    size_t onPopStart(WorkPoolBase &, WorkerBase &, size_t _maxcnt){
        return _maxcnt;
    }
    void onPopDone(WorkPoolBase &, WorkerBase &){
    }
    void onStop(){
    }
};

//! A generic workpool
/*!
 * The template parameters are:<br>
 * J - the Job type to be processed by the workpool. I
 * will hold a Queue\<J\>.<br>
 * C - WorkPool controller, the workpool will call controller
 * methods on different ocasions / events<br>
 * W - Base class for workers. Specify this if you want certain data
 * kept per worker. The workpool's actual workers will publicly
 * inherit W.<br>
 */
template <class J, class C, class W = WorkerBase>
class WorkPool: public WorkPoolBase{
    
    typedef std::vector<J>              JobVectorT;
    typedef WorkPool<J, C, W>           ThisT;
    typedef std::vector<std::thread>    ThreadVectorT;
    
    
    struct SingleWorker: W{
        SingleWorker(ThisT &_rw):rw(_rw){}
        void run(){
            if(!rw.enterWorker(*this)){
                return;
            }
            J   job;
            while(rw.pop(*this, job)){
                rw.execute(*this, job);
            }
            rw.exitWorker(*this);
        }
        ThisT   &rw;
    };
    struct MultiWorker: W{
        
        MultiWorker(ThisT &_rw, size_t _maxcnt):rw(_rw), maxcnt(_maxcnt){}
        
        void run(){
            if(!rw.enterWorker(*this)){
                return;
            }
            JobVectorT  jobvec;
            if(maxcnt == 0) maxcnt = 1;
            while(rw.pop(*this, jobvec, maxcnt)){
                rw.execute(*this, jobvec);
                jobvec.clear();
            }
            rw.exitWorker(*this);
        }
        ThisT   &rw;
        size_t  maxcnt;
    };
    
    static void single_worker_run(ThisT *_pthis){
        SingleWorker wkr(*_pthis);
        wkr.run();
    }
    
    static void multi_worker_run(ThisT *_pthis, const size_t _cnt){
        MultiWorker wkr(*_pthis, _cnt);
        wkr.run();
    }
    
public:
    typedef ThisT   WorkPoolT;
    typedef C       ControllerT;
    typedef W       WorkerT;
    typedef J       JobT;
    
    
    WorkPool(){
    }
    
    template <class T>
    WorkPool(T &_rt):ctrl(_rt){
    }
    
    ~WorkPool(){
        stop(true);
    }
    
    ControllerT& controller(){
        return ctrl;
    }
    
    //! Push a new job
    void push(const JobT& _jb){
        std::unique_lock<std::mutex>    lock(mtx);
        jobq.push(_jb);
        sigcnd.notify_one();
        ctrl.onPush(*this);
    }
    //! Push a table of jobs of size _cnt
    template <class I>
    void push(I _i, const I &_end){
        std::unique_lock<std::mutex>    lock(mtx);
        size_t cnt(_end - _i);
        for(; _i != _end; ++_i){
            jobq.push(*_i);
        }
        sigcnd.notify_all();
        ctrl.onMultiPush(*this, cnt);
    }
    
    //! Starts the workpool, creating _minwkrcnt
    void start(ushort _minwkrcnt = 0){
        std::unique_lock<std::mutex>    lock(mtx);
        if(state() == Running){
            return;
        }
        if(state() != Stopped){
            doStop(lock, true);
        }
        wkrcnt = 0;
        state(Running);
        for(ushort i(0); i < _minwkrcnt; ++i){
            createWorker();
        }
    }
    //! Initiate workpool stop
    /*!
        It can block waiting for all workers to stop or just initiate stopping.
        The ideea is that when one have M workpools, it is faster to
        first initiate stop for all pools (wp[i].stop(false)) and then wait
        for actual stoping (wp[i].stop(true))
    */
    void stop(bool _wait = true){
        std::unique_lock<std::mutex>    lock(mtx);
        doStop(lock, _wait);
    }
    size_t size()const{
        return jobq.size();
    }
    bool empty()const{
        return jobq.empty();
    }
    
    void createWorker(){
        ++wkrcnt;
        
        static const std::thread    empty_thread{};
        
        thread_vec.push_back(std::move(std::thread()));
        
        ctrl.createWorker(*this, wkrcnt, thread_vec.back());
        
        if(thread_vec.back().get_id() == empty_thread.get_id()){
            --wkrcnt;
            thread_vec.pop_back();
            thrcnd.notify_all();
        }
    }
    
    void createSingleWorker(std::thread &_rthr){
        try{
            _rthr = std::thread(single_worker_run, this);
        }catch(...){
            _rthr.join();
            _rthr = std::thread();
        }
    }
    
    void createMultiWorker(std::thread &_rthr, size_t _maxcnt){
        try{
            _rthr = std::thread(multi_worker_run, this, _maxcnt);
        }catch(...){
            _rthr.join();
            _rthr = std::thread();
        }
    }
    
private:
    friend struct SingleWorker;
    friend struct MultiWorker;
    
    void doStop(std::unique_lock<std::mutex> &_lock, bool _wait){
        if(state() == Stopped) return;
        state(Stopping);
        sigcnd.notify_all();
        ctrl.onStop();
        if(!_wait) return;
        while(wkrcnt){
            thrcnd.wait(_lock);
        }
        for(std::thread &thr: thread_vec){
            thr.join();
        }
        thread_vec.clear();
        state(Stopped);
    }
    
    bool pop(WorkerT &_rw, JobVectorT &_rjobvec, size_t _maxcnt){
        std::unique_lock<std::mutex>    lock(mtx);
        
        uint32_t insertcount(ctrl.onPopStart(*this, _rw, _maxcnt));
        if(!insertcount){
            return true;
        }
        if(doWaitJob(lock)){
            do{
                _rjobvec.push_back(jobq.front());
                jobq.pop();
            }while(jobq.size() && --insertcount);
            ctrl.onPopDone(*this, _rw);
            return true;
        }
        return false;
    }
    
    bool pop(WorkerT &_rw, JobT &_rjob){
        
        std::unique_lock<std::mutex>    lock(mtx);
        
        if(ctrl.onPopStart(*this, _rw, 1) == 0){
            sigcnd.notify_one();
            return false;
        }
        if(doWaitJob(lock)){
            _rjob = jobq.front();
            jobq.pop();
            ctrl.onPopDone(*this, _rw);
            return true;
        }
        return false;
    }
    
    size_t doWaitJob(std::unique_lock<std::mutex> &_lock){
        while(jobq.empty() && isRunning()){
            sigcnd.wait(_lock);
        }
        return jobq.size();
    }
    
    bool enterWorker(WorkerT &_rw){
        std::unique_lock<std::mutex>    lock(mtx);
        if(!ctrl.prepareWorker(_rw)){
            return false;
        }
        //++wkrcnt;
        thrcnd.notify_all();
        return true;
    }
    void exitWorker(WorkerT &_rw){
        std::unique_lock<std::mutex>    lock(mtx);
        ctrl.unprepareWorker(_rw);
        --wkrcnt;
        SOLID_ASSERT(wkrcnt >= 0);
        thrcnd.notify_all();
    }
    void execute(WorkerT &_rw, JobT &_rjob){
        ctrl.execute(*this, _rw, _rjob);
    }
    void execute(WorkerT &_rw, JobVectorT &_rjobvec){
        ctrl.execute(*this, _rw, _rjobvec);
    }
private:
    Queue<JobT>                 jobq;
    ControllerT                 ctrl;
    ThreadVectorT               thread_vec;
};

}//namespace solid

#endif



