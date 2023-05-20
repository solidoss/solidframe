#include "solid/utility/event.hpp"
#include <set>
#include <vector>

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

template <>
const GlobalEventCategory solid::category<GlobalEvents>{
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
template <>
const AlphaEventCategory solid::category<AlphaEvents>{
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

template <>
const BetaEventCategory solid::category<BetaEvents>{
    "::beta_event_category",
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
        {{make_event(GlobalEvents::First),
             [](EventBase& _revt, Object& _robj) { cout << "handle_global_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(AlphaEvents::First),
                [](EventBase& _revt, Object& _robj) { cout << "handle_alpha_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(AlphaEvents::Second),
                [](EventBase& _revt, Object& _robj) { cout << "handle_alpha_second on " << &_robj << " for " << _revt << endl; }},
            {make_event(BetaEvents::First),
                [](EventBase& _revt, Object& _robj) { cout << "handle_beta_first on " << &_robj << " for " << _revt << endl; }},
            {make_event(BetaEvents::Third),
                [](EventBase& _revt, Object& _robj) { cout << "handle_beta_third on " << &_robj << " for " << _revt << endl; }},
            {generic_event<GenericEventE::Message>,
                [](EventBase& _revt, Object& _robj) { cout << "handle_generic_message on " << &_robj << " for " << _revt << " data = " << *_revt.cast<std::string>() << endl; }}}};

    event_handler.handle(_revt, *this);
}

int test_event(int /*argc*/, char* /*argv*/[])
{

    Object obj;

    Base& rbase(obj);

    rbase.handleEvent(make_event(GlobalEvents::First));
    rbase.handleEvent(make_event(AlphaEvents::Second));
    rbase.handleEvent(make_event(BetaEvents::Third));
    rbase.handleEvent(make_event(BetaEvents::Second));
    rbase.handleEvent(make_event(GenericEventE::Message, std::string("Some text")));

    auto tuple_event = make_event(BetaEvents::Second, std::make_tuple(to_string(10), vector<int>{1, 2, 3, 4}, set<std::string>{"a", "b", "c"}));

    auto check_event = [](EventBase& _event) {
        solid_check(_event.get_if<string>());
        solid_check(*_event.get_if<string>() == to_string(10));
        auto* pvec = _event.get_if<vector<int>>();
        solid_check(pvec && pvec->size() == 4 && pvec->at(3) == 4);

        auto* pset = _event.get_if<set<string>>();
        solid_check(pset && pset->size() == 3 && pset->find("b") != pset->end());
    };

    check_event(tuple_event);

    Event<8> tuple_event8(tuple_event);
    check_event(tuple_event8);
    check_event(tuple_event);

    {
        Event<8> tuple_event8(std::move(tuple_event));
        check_event(tuple_event8);
        solid_check(!tuple_event);
    }

    tuple_event = tuple_event8;
    check_event(tuple_event8);
    check_event(tuple_event);

    Event<10> tuple_event10;

    tuple_event10 = std::move(tuple_event);
    solid_check(!tuple_event);
    check_event(tuple_event10);
    return 0;
}
