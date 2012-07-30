#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/iterator/iterator_concepts.hpp>

#include <iostream>
#include <deque>
#include <vector>
using namespace std;

struct Test{
    int v;
    Test(int _v = 0):v(_v){}
    
    int test(int _x){
        return v * _x;
    }
};

typedef boost::shared_ptr<Test>     TestSharedPtrT;
typedef deque<TestSharedPtrT>       TestDequeT;
typedef vector<TestSharedPtrT>      TestVectorT;
typedef deque<boost::thread>        ThreadVectorT;

struct Runner{
    void operator()();
};

static TestDequeT testdeq;

int main(int argc, char *argv[]){
    if(argc != 4){
        cout<<"Use: ./example_testshare repeatcnt objcnt thrcnt"<<endl;
    }
    int repcnt = atoi(argv[1]);
    int objcnt = atoi(argv[2]);
    int thrcnt = atoi(argv[3]);
    
    ThreadVectorT thrvec;
    
    while(--repcnt){
        for(int i = 0; i < objcnt; ++i){
            TestSharedPtrT sp(new Test(i));
            testdeq.push_back(sp);
        }
        
        for(int i = 0; i < thrcnt; ++i){
            thrvec.push_back(boost::thread(Runner()));
        }
    }
   
}

void Runner::operator()(){
    TestVectorT  testvec;
    for(TestDequeT::const_iterator it(testdeq.begin()); it != testdeq.end(); ++it){
        testvec.push_back(*it);
    }
    
    for(TestVectorT::const_iterator it(testvec.begin()); it != testvec.end(); ++it){
        (*it)->test(it - testvec.begin());
    }
}
