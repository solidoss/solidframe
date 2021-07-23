#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"
#include <functional>

using namespace std;
using namespace solid;

int test_event_size(int argc, char* argv[])
{

    cout << "sizeof(Event) = " << sizeof(Event) << endl;
    cout << "sizeof(Event::any) = " << sizeof(Event::AnyT) << endl;
    cout << "Event::any::smallCapacity = " << Event::AnyT::smallCapacity() << endl;
    cout << "sizeof(Function<void()>) = " << sizeof(Function<void()>) << endl;
    cout << "sizeof(std::function<void()>) = " << sizeof(std::function<void()>) << endl;
    static_assert(Event::AnyT::smallCapacity() >= sizeof(Function<void()>));
    static_assert(Event::AnyT::smallCapacity() >= sizeof(std::function<void()>));

    return 0;
}
