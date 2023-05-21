#include "solid/system/log.hpp"
#include "solid/utility/event.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/queue.hpp"
#include <iostream>
#include <queue>

using namespace solid;
using namespace std;

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

typedef solid_function_t(void(size_t&, EventBase&&)) EventFunctionT;

struct ExecStub {
    using EventT = Event<256>;
    UniqueId       actuid;
    UniqueId       chnuid;
    EventFunctionT exefnc;
    EventT         event;

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, EventT&& _uevent = EventT())
        : actuid(_ruid)
        , exefnc(_f)
        , event(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, EventT&& _uevent = EventT())
        : actuid(_ruid)
        , chnuid(_rchnuid)
        , exefnc(std::move(_f))
        , event(std::move(_uevent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, EventT&& _uevent = EventT())
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
};

int test_queue(int args, char* argv[])
{

    solid::log_start(std::cerr, {".*:VIEWXS"});

    using EventQueueT = Queue<ExecStub>;
    // using EventQueueT = queue<Event>;
    EventQueueT eventq;

    size_t v = 0;
    for (int i = 0; i < 100; ++i) {
        eventq.push(ExecStub(
            UniqueId(i, i),
            [](size_t& _sz, EventBase&& _evt) {
                ++_sz;
                cout << _sz << " eventdata: " << *_evt.cast<std::string>() << endl;
            },
            make_event(GenericEventE::Message, to_string(i))));
    }

    while (!eventq.empty()) {
        eventq.front().exefnc(v, Event<128>(eventq.front().event));
        eventq.pop();
    }
    cout << "v = " << v << endl;
    return 0;
}
