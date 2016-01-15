
#if 0

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
#else
#include "system/debug.hpp"
#include <boost/any.hpp>

#include <queue>
#include <iostream>
#include <vector>

using namespace solid;
using namespace std;


class EventCategoryBase;

struct Event{
	Event();
	Event(Event &&);
	
	Event& operator=(Event&&);
	
	template <typename Map, typename DefCbk>
	void handle(const Map &_handle_map, DefCbk _cbk){
		size_t idx;
		if(_handle_map.find(*this, idx)){
			_handle_map.call(idx, *this);
		}else{
			_cbk(*this);
		}
	}
private:
	friend class EventCategoryBase;
	Event(
		const size_t _id,
		const EventCategoryBase &_rcategory
	):pcategory_(&_rcategory), id_(_id){}
private:
	Event(const Event &) = delete;
	Event& operator=(const Event &) = delete;
private:
	const EventCategoryBase	*pcategory_;
	size_t					id_;
};


class EventCategoryBase{
protected:
	virtual ~EventCategoryBase(){}
	Event event(const size_t _idx)const{
		return Event(_idx, *this);
	}
};


template <typename EventIds>
class EventCategory: public EventCategoryBase{
public:
	Event event(const EventIds _id)const{
		return EventCategoryBase::event(static_cast<size_t>(_id));
	}
};


class Base{
public:
	virtual ~Base(){}
	virtual void handleEvent(Event &&_revt) = 0;
};

class Object: public Base{
private:
	/*virtual*/ void handleEvent(Event &&_revt)override;
};


enum struct GlobalEvents{
	First,
	Second,
	Third,
};

enum struct AlphaEvents{
	First,
	Second,
	Third,
};

enum struct BetaEvents{
	First,
	Second,
	Third,
};


using GlobalEventCategory = EventCategory<GlobalEvents>;
using AlphaEventCategory = EventCategory<AlphaEvents>;
using BetaEventCategory = EventCategory<BetaEvents>;

const GlobalEventCategory	global_event_category;
const AlphaEventCategory	alpha_event_category;
const BetaEventCategory		beta_event_category;

void Object::handleEvent(Event &&_revt){
#if 0
	static EventHandlerMap cbk_map = {
		{
			Event(),
			[](Object &_robj, Event &_revt){cout<<"handle_invalid_event on "<<&_robj<<" for "<<_revt;}
		},
		{
			global_event_category.event(GlobalEvents::First),
			[](Object &_robj, Event &_revt){cout<<"handle_global_first on "<<&_robj<<" for "<<_revt;}
		},
		{
			alpha_event_category.event(AlphaEvents::First),
			[](Object &_robj, Event &_revt){cout<<"handle_alpha_first on "<<&_robj<<" for "<<_revt;}
		},
		{
			alpha_event_category.event(AlphaEvents::Second),
			[](Object &_robj, Event &_revt){cout<<"handle_alpha_second on "<<&_robj<<" for "<<_revt;}
		},
		{
			beta_event_category.event(BetaEvents::First),
			[](Object &_robj, Event &_revt){cout<<"handle_beta_first on "<<&_robj<<" for "<<_revt;}
		},
		{
			beta_event_category.event(BetaEvents::Third),
			[](Object &_robj, Event &_revt){cout<<"handle_beta_third on "<<&_robj<<" for "<<_revt;}
		}
	};
	
	_revt.handle(cbk_map, [&](Event &_revt){cout<<"Unknown event"<<endl;});
#endif
}

int main(int argc, char *argv[]){
	
	boost::any a;
	cout<<"sizeof(a) = "<<sizeof(a)<<endl;
	
	Object 	obj;
	
	Base	&rbase(obj);
	
	rbase.handleEvent(global_event_category.event(GlobalEvents::First));
	rbase.handleEvent(alpha_event_category.event(AlphaEvents::Second));
	rbase.handleEvent(beta_event_category.event(BetaEvents::Third));
	
	return 0;
}
#endif