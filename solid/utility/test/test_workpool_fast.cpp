#include "solid/utility/workpool.hpp"
#include "solid/system/exception.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

using namespace solid;
using namespace std;

std::atomic<size_t>         val{0};

class WPController: public WorkPoolControllerBase{
public:
    const size_t max_thr_cnt_;
    
    WPController(const size_t _max_thr_cnt):max_thr_cnt_(_max_thr_cnt){}
    
    template <class WP>
    bool createWorker(WP &_rwp, size_t /*_wkrcnt*/, std::thread &_rthr){
        _rwp.createSingleWorker(_rthr);
        return true;
    }
    
    template <class WP>
    void onPush(WP &_rwp, size_t _qsz){
        if(_qsz > _rwp.workerCount() and _rwp.workerCount() < max_thr_cnt_){
            _rwp.createWorker();
        }
    }
    
    void execute(WorkPoolBase &/*_rwp*/, WorkerBase &/*_rw*/, int v){
        val += v;
    }
};

typedef WorkPool<int, WPController> MyWorkPoolT;

int test_workpool_fast(int argc, char *argv[]){
    MyWorkPoolT                 wp{5};
    const size_t                cnt{5000000};
    
    wp.start(5);
    
    cout<<"wp started"<<endl;
    
    for(int i = 0; i < cnt; ++i){
        wp.push(i);
    };
    
    wp.stop();
    cout<<"val = "<<val<<endl;
    const size_t v = ((cnt - 1) * cnt)/2;
    
    SOLID_CHECK(v == val);
    return 0;
}
