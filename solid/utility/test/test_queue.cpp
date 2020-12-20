#include <queue>
#include <iostream>
#include "solid/system/log.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/event.hpp"

using namespace solid;
using namespace std;

struct Node{
    using Storage = typename std::aligned_storage<sizeof(Event), alignof(Event)>::type;
    //unsigned char data[sizeof(Event)];
    Storage  data[10];
    void * pv_;
};

int test_queue(int args, char *argv[]){
    
    solid::log_start(std::cerr, {".*:VIEWXS"});
    
    using EventQueueT = Queue<Event>;
    //using EventQueueT = queue<Event>;
    EventQueueT eventq;
    
    //void * pv;
    //unsigned char buf[sizeof(Event)];
    
    auto pn = new Node;
    
    cout<<alignof(Event)<<endl;
    cout<<pn<<endl;
    cout<<static_cast<void*>(&pn->data[0])<<endl;
    cout<<std::launder(reinterpret_cast<Event*>(&pn->data[0]))<<endl;
    
    new (std::launder(reinterpret_cast<Event*>(&pn->data[0]))) Event(make_event(GenericEvents::Default));
    
    //new (std::launder(reinterpret_cast<Event*>(&pn->data[0]))) Event(make_event(GenericEvents::Default));
    
    for(int i = 0; i < 100; ++i){
        eventq.push(make_event(GenericEvents::Default));
    }
    
    return 0;
}
