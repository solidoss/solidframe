#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/iterator/iterator_concepts.hpp>

#include "system/sharedbackend.hpp"

#include "system/debug.hpp"

#include <iostream>
#include <deque>
#include <vector>
using namespace std;

struct Test{
    int v;
    Test(int _v = 0):v(_v){
		idbg("");
	}
	~Test(){
		idbg("");
	}
    
    int test(int _x){
        return v * _x;
    }
};

template <class T>
class SharedPtr{
public:
    static void delT(void *_pv){
        delete reinterpret_cast<T*>(_pv);
    }
    typedef SharedPtr<T> ThisT;
    SharedPtr():pss(NULL){}
    
    SharedPtr(T *_pt):pss(NULL){
        pss = SharedBackend::the().create(_pt, &delT);
    }
    
    SharedPtr(const ThisT& _rptr):pss(NULL){
        pss = _rptr.pss;
		if(pss){
			SharedBackend::the().use(*pss);
		}
    }
    ~SharedPtr(){
        if(!empty()){
            release();
        }
    }
    
    ThisT& operator=(const ThisT&_rptr){
        if(!empty()){
            release();
        }
        pss = _rptr.pss;
        SharedBackend::the().use(*pss);
        return *this;
    }
    
    void release(){
        SharedBackend::the().release(*pss);
		pss = NULL;
    }
    
    bool empty()const{
        return pss == NULL;
    }
    T* ptr()const{
        return reinterpret_cast<T*>(pss->ptr);
    }
    
    T* operator->()const{
        return ptr();
    }
private:
    mutable SharedStub *pss;
};


//typedef boost::shared_ptr<Test>     TestSharedPtrT;
typedef SharedPtr<Test>             TestSharedPtrT;
typedef deque<TestSharedPtrT>       TestDequeT;
typedef vector<TestSharedPtrT>      TestVectorT;
typedef deque<boost::thread*>       ThreadVectorT;

struct Runner{
    void operator()();
};

static TestDequeT testdeq;

int main(int argc, char *argv[]){
    if(argc != 4){
        cout<<"Use: ./example_testshare repeatcnt objcnt thrcnt"<<endl;
        return 0;
    }
#ifdef UDEBUG
    std::string s;
    Dbg::instance().levelMask("view");
    Dbg::instance().moduleMask("any");
    Dbg::instance().initStdErr(
        false,
        &s
    );
#endif
    int repcnt = atoi(argv[1]);
    int objcnt = atoi(argv[2]);
    int thrcnt = atoi(argv[3]);
    
    ThreadVectorT thrvec;
    
    while(repcnt--){
        idbg("create "<<objcnt<<" objects:");
        for(int i = 0; i < objcnt; ++i){
            TestSharedPtrT sp(new Test(i));
            testdeq.push_back(sp);
        }
        idbg("create "<<thrcnt<<" threads:");
        for(int i = 0; i < thrcnt; ++i){
            Runner          r;
            boost::thread   *pthr = new boost::thread(r);
            thrvec.push_back(pthr);
        }
        idbg("wait for all threads");
        for(int i = 0; i < thrcnt; ++i){
            thrvec[i]->join();
            delete thrvec[i];
        }
        thrvec.clear();
        testdeq.clear();
    }
    return 0;
}

void Runner::operator()(){
    TestVectorT  testvec;
    //idbg("thread started");
    for(TestDequeT::const_iterator it(testdeq.begin()); it != testdeq.end(); ++it){
        testvec.push_back(*it);
    }
    int i = 0;
    for(TestVectorT::const_iterator it(testvec.begin()); it != testvec.end(); ++it){
        i += (*it)->test(it - testvec.begin());
    }
    //idbg("thread done "<<i);
}
