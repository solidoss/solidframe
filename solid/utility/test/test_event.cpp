#include "solid/utility/event.hpp"

using namespace std;
using namespace solid;

enum struct GlobalEvents {
    First,
    Second,
    Third,
};

enum struct AlphaEvents {
    First,
    Second,
    Third,
};

enum struct BetaEvents {
    First,
    Second,
    Third,
};

using GlobalEventCategory = EventCategory<GlobalEvents>;
using AlphaEventCategory  = EventCategory<AlphaEvents>;
using BetaEventCategory   = EventCategory<BetaEvents>;

const GlobalEventCategory global_event_category{
    "::global_event_category",
    [](const GlobalEvents _evt) {
        switch (_evt) {
        case GlobalEvents::First:
            return "first";
        case GlobalEvents::Second:
            return "second";
        case GlobalEvents::Third:
            return "third";
        default:
            return "unknown";
        }
    }};
const AlphaEventCategory alpha_event_category{
    "::alpha_event_category",
    [](const AlphaEvents _evt) {
        switch (_evt) {
        case AlphaEvents::First:
            return "first";
        case AlphaEvents::Second:
            return "second";
        case AlphaEvents::Third:
            return "third";
        default:
            return "unknown";
        }
    }};
const BetaEventCategory beta_event_category{
    "::alpha_event_category",
    [](const BetaEvents _evt) {
        switch (_evt) {
        case BetaEvents::First:
            return "first";
        case BetaEvents::Second:
            return "second";
        case BetaEvents::Third:
            return "third";
        default:
            return "unknown";
        }
    }};

class Base {
public:
    virtual ~Base() {}
    virtual void handleEvent(EventBase&& _revt) = 0;
};

class Object : public Base {
private:
    /*virtual*/ void handleEvent(EventBase&& _revt) override;
};

void Object::handleEvent(EventBase&& _revt)
{
    static const EventHandler<void, Object&> event_handler = {
        [](EventBase& _revt, Object& _robj) { cout << "handle_invalid_event on " << &_robj << " for " << _revt << endl; },
        {{make_event(global_event_category, GlobalEvents::First),
             [](EventBase& _revt, Object& _robj) { cout << "handle_global_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(alpha_event_category, AlphaEvents::First),
                [](EventBase& _revt, Object& _robj) { cout << "handle_alpha_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(alpha_event_category, AlphaEvents::Second),
                [](EventBase& _revt, Object& _robj) { cout << "handle_alpha_second on " << &_robj << " for " << _revt << endl; }},
            {make_event(beta_event_category, BetaEvents::First),
                [](EventBase& _revt, Object& _robj) { cout << "handle_beta_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(beta_event_category, BetaEvents::Third),
                [](EventBase& _revt, Object& _robj) { cout << "handle_beta_third on " << &_robj << " for " << _revt << endl; }},
            {generic_event_message,
                [](EventBase& _revt, Object& _robj) { cout << "handle_generic_message on " << &_robj << " for " << _revt << " any value = " << *_revt.cast<std::string>() << endl; }}}};

    event_handler.handle(_revt, *this);
}

int test_event(int /*argc*/, char* /*argv*/[])
{

    Object obj;

    Base& rbase(obj);

    rbase.handleEvent(make_event(global_event_category, GlobalEvents::First));
    rbase.handleEvent(make_event(alpha_event_category, AlphaEvents::Second));
    rbase.handleEvent(make_event(beta_event_category, BetaEvents::Third));
    rbase.handleEvent(make_event(beta_event_category, BetaEvents::Second));
    rbase.handleEvent(make_event(generic_event_category, GenericEvents::Message, std::string("Some text")));

    return 0;
}
