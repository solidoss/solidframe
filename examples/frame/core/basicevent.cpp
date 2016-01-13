#include "frame/event.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"

#include <queue>
#include <iostream>
#include <vector>

using namespace solid;
using namespace std;


struct ACategory: Dynamic<ACategory, solid::frame::EventCategory>{
	enum EventId{
		Event1E,
		Event2E,
		Event3E,
		Event4E,
	};
	
	EventId id(frame::Event const&_re){
		return static_cast<EventId>(eventId(_re));
	}
	
	frame::Event create(EventId _evid)const{
		return doCreate(static_cast<size_t>(_evid));
	}
};


struct BCategory: Dynamic<BCategory, solid::frame::EventCategory>{
	enum EventId{
		Event1E,
		Event2E,
		Event3E,
		Event4E,
	};
	
	EventId id(frame::Event const&_re){
		return static_cast<EventId>(eventId(_re));
	}
	
	frame::Event create(EventId _evid)const{
		return doCreate(static_cast<size_t>(_evid));
	}
};

struct CCategory: Dynamic<CCategory, solid::frame::EventCategory>{
	enum EventId{
		Event1E,
		Event2E,
		Event3E,
		Event4E,
	};
	
	EventId id(frame::Event const&_re){
		return static_cast<EventId>(eventId(_re));
	}
	
	frame::Event create(EventId _evid)const{
		return doCreate(static_cast<size_t>(_evid));
	}
};

static const ACategory	acategory;
static const BCategory	bcategory;
static const CCategory	ccategory;

typedef std::queue<frame::Event>		EventQueueT;

int main(int argc, char *argv[]){
	if(argc < 3){
		cout<<"./example_basicevent 1000 100"<<endl;
		return 0;
	}
	int cnt = atoi(argv[1]);
	int rep = atoi(argv[2]);
	
	EventQueueT		eq;
	vector<int>		iv(4);
	for(int j = 0; j < rep; ++j){
		for(int i = 0; i < cnt; ++i){
			eq.push(acategory.create(ACategory::Event1E).assign(i));
		}
		
		while(eq.size()){
			
		}
	}
	return 0;
}

