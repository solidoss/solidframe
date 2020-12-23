
using namespace std;
using namespace solid;

#ifndef SOLID_USE_STD_FUNCTION
using EventFunctionT = solid::Function<void(const UniqueId&, Event&&), 56>;
#else
using EventFunctionT = std::function<void(const UniqueId&, Event&&)>;
#endif

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

    UniqueId                actuid;
    UniqueId                chnuid;
    EventFunctionT          exefnc;
    Event                   event;
};

#ifdef USE_STD_QUEUE
using ExecQueueT = queue<ExecStub>;
#else
using ExecQueueT = Queue<ExecStub>;
#endif



int test_function_any_speed(int argc, char *argv[]){
    cout<<"sizeof(ExecStub) = "<<sizeof(ExecStub)<<endl;
    cout<<"sizeof(Event) = "<<sizeof(Event)<<endl;
    cout<<"sizeof(EventFunctionT) = "<<sizeof(EventFunctionT)<<endl;
    size_t repeat_count = 100;
    size_t insert_count = 100000;

    ExecQueueT exeq;
    uint64_t   result = 0;
    for(size_t j = 0; j < repeat_count; ++j){
        for(size_t i = 0; i < insert_count; ++i){
            exeq.push(
                ExecStub{
                    UniqueId{i,static_cast<UniqueT>(j)},
                    [&result, str = to_string(i)](const UniqueId &_ruid, Event &&_revent){
                        result += _ruid.index;
#ifdef SOLID_EVENT_USE_STD_ANY
                        result += *std::any_cast<size_t>(&_revent.any());
#else
                        result += *_revent.any().cast<size_t>();
#endif
                        result += str.size();
                    },
                    UniqueId{j,static_cast<UniqueT>(i)},
                    make_event(GenericEvents::Default, i)
                }
            );
        }
        while(!exeq.empty()){
            auto &rexe = exeq.front();
            rexe.exefnc(rexe.actuid, std::move(rexe.event));
            exeq.pop();
        }

    }

    cout << result<<endl;

    return 0;
}
