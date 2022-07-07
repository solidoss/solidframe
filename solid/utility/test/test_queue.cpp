#include "solid/system/log.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/queue.hpp"
#include <iostream>
#include <queue>

using namespace solid;
using namespace std;

struct Node {
    using Storage = typename std::aligned_storage<sizeof(Event), alignof(Event)>::type;
    // unsigned char data[sizeof(Event)];
    Storage data[10];
    void*   pv_;
};

using IndexT  = uint64_t;
using UniqueT = uint32_t;

struct UniqueId {
    IndexT  index;
    UniqueT unique;

    static UniqueId invalid()
    {
        return UniqueId();
    }

    UniqueId(
        IndexT const& _idx = InvalidIndex(),
        UniqueT       _unq = InvalidIndex())
        : index(_idx)
        , unique(_unq)
    {
    }

    bool isInvalid() const
    {
        return index == InvalidIndex();
    }
    bool isValid() const
    {
        return !isInvalid();
    }

    bool operator==(UniqueId const& _ruid) const
    {
        return _ruid.index == this->index && _ruid.unique == this->unique;
    }
    bool operator!=(UniqueId const& _ruid) const
    {
        return _ruid.index != this->index || _ruid.unique != this->unique;
    }
    void clear()
    {
        index  = InvalidIndex();
        unique = InvalidIndex();
    }
};

typedef solid_function_t(void(size_t&, Event&&)) EventFunctionT;

struct ExecStub {
    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, Event&& _uevent = Event())
        : actuid(_ruid)
        , exefnc(_f)
        , event(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, Event&& _uevent = Event())
        : actuid(_ruid)
        , chnuid(_rchnuid)
        , exefnc(std::move(_f))
        , event(std::move(_uevent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event&& _uevent = Event())
        : actuid(_ruid)
        , event(std::move(_uevent))
    {
    }

    ExecStub(const ExecStub&) = delete;

    ExecStub(
        ExecStub&& _res) noexcept
        : actuid(_res.actuid)
        , chnuid(_res.chnuid)
        , event(std::move(_res.event))
    {
        std::swap(exefnc, _res.exefnc);
    }

    UniqueId       actuid;
    UniqueId       chnuid;
    EventFunctionT exefnc;
    Event          event;
};

int test_queue(int args, char* argv[])
{

    solid::log_start(std::cerr, {".*:VIEWXS"});

    using EventQueueT = Queue<ExecStub>;
    // using EventQueueT = queue<Event>;
    EventQueueT eventq;

    // void * pv;
    // unsigned char buf[sizeof(Event)];

    auto pn = new Node;

    cout << alignof(Event) << endl;
    cout << pn << endl;
    cout << static_cast<void*>(&pn->data[0]) << endl;
    cout << std::launder(reinterpret_cast<Event*>(&pn->data[0])) << endl;

    new (std::launder(reinterpret_cast<Event*>(&pn->data[0]))) Event(make_event(GenericEvents::Default));

    // new (std::launder(reinterpret_cast<Event*>(&pn->data[0]))) Event(make_event(GenericEvents::Default));

    size_t v = 0;
    for (int i = 0; i < 100; ++i) {
        eventq.push(ExecStub(
            UniqueId(i, i), [](size_t& _sz, Event&& _evt) { ++_sz; }, make_event(GenericEvents::Default)));
    }

    while (!eventq.empty()) {
        eventq.front().exefnc(v, Event(eventq.front().event));
        eventq.pop();
    }
    cout << "v = " << v << endl;
    return 0;
}
