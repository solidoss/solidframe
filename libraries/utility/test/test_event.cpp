#include "utility/event.hpp"

using namespace std;
using namespace solid;

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

const GlobalEventCategory	global_event_category{
	"::global_event_category",
	[](const GlobalEvents _evt){
		switch(_evt){
			case GlobalEvents::First:
				return "first";
			case GlobalEvents::Second:
				return "second";
			case GlobalEvents::Third:
				return "third";
			default:
				return "unknown";
		}
	}
};
const AlphaEventCategory	alpha_event_category{
	"::alpha_event_category",
	[](const AlphaEvents _evt){
		switch(_evt){
			case AlphaEvents::First:
				return "first";
			case AlphaEvents::Second:
				return "second";
			case AlphaEvents::Third:
				return "third";
			default:
				return "unknown";
		}
	}
};
const BetaEventCategory		beta_event_category{
	"::alpha_event_category",
	[](const BetaEvents _evt){
		switch(_evt){
			case BetaEvents::First:
				return "first";
			case BetaEvents::Second:
				return "second";
			case BetaEvents::Third:
				return "third";
			default:
				return "unknown";
		}
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


void Object::handleEvent(Event &&_revt){
	static EventHandler<void, Object&> event_handler = {
		[](Event &_revt, Object &_robj){cout<<"handle_invalid_event on "<<&_robj<<" for "<<_revt<<endl;},
		{
			{
				global_event_category.event(GlobalEvents::First),
				[](Event &_revt, Object &_robj){cout<<"handle_global_first on "<<&_robj<<" for "<<_revt<<endl;}
			},
			{
				alpha_event_category.event(AlphaEvents::First),
				[](Event &_revt, Object &_robj){cout<<"handle_alpha_first on "<<&_robj<<" for "<<_revt<<endl;}
			},
			{
				alpha_event_category.event(AlphaEvents::Second),
				[](Event &_revt, Object &_robj){cout<<"handle_alpha_second on "<<&_robj<<" for "<<_revt<<endl;}
			},
			{
				beta_event_category.event(BetaEvents::First),
				[](Event &_revt, Object &_robj){cout<<"handle_beta_first on "<<&_robj<<" for "<<_revt<<endl;}
			},
			{
				beta_event_category.event(BetaEvents::Third),
				[](Event &_revt, Object &_robj){cout<<"handle_beta_third on "<<&_robj<<" for "<<_revt<<endl;}
			}
		}
	};
	
	event_handler.handle(_revt, *this);
}

int test_event(int argc, char *argv[]){
	
	cout<<"Event::any_size = "<<Event::any_size<<endl;
	
	Object 	obj;
	
	Base	&rbase(obj);
	
	rbase.handleEvent(global_event_category.event(GlobalEvents::First));
	rbase.handleEvent(alpha_event_category.event(AlphaEvents::Second));
	rbase.handleEvent(beta_event_category.event(BetaEvents::Third));
	rbase.handleEvent(beta_event_category.event(BetaEvents::Second));
	rbase.handleEvent(Event());
	
	return 0;
}