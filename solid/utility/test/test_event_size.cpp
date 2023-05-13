#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"
#include <functional>

using namespace std;
using namespace solid;

int test_event_size(int argc, char* argv[])
{

    cout << "sizeof(Event) = " << sizeof(Event<>) << endl;
    cout << "sizeof(Function<void()>) = " << sizeof(Function<void()>) << endl;
    cout << "sizeof(std::function<void()>) = " << sizeof(std::function<void()>) << endl;
    auto evt_std_fnc   = make_event(GenericEventE::Message, std::function<void()>{[]() {}});
    auto evt_solid_fnc = make_event(GenericEventE::Message, Function<void()>{[]() {}});
    static_assert(decltype(evt_solid_fnc)::smallCapacity() >= sizeof(Function<void()>));
    static_assert(decltype(evt_std_fnc)::smallCapacity() >= sizeof(std::function<void()>));

    return 0;
}
