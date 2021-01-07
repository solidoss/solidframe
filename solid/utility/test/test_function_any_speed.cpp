
using namespace std;
using namespace solid;

struct Event;

#ifndef SOLID_USE_STD_FUNCTION
using EventFunctionT = solid::Function<void(const UniqueId&, Event&&), 56>;
#else
using EventFunctionT = std::function<void(const UniqueId&, Event&&)>;
#endif

//-----------------------------------------------------------------------------
//      Event
//-----------------------------------------------------------------------------

struct Event {
#ifdef EVENT_USE_STD_ANY
    using AnyT = std::any;
#else
    static constexpr size_t any_size = sizeof(void*) == 8 ? any_size_from_sizeof(64 - sizeof(void*) - sizeof(uintptr_t)) : any_size_from_sizeof(32 - sizeof(void*) - sizeof(uintptr_t));
    using AnyT                       = Any<any_size>;
#endif
    Event();
    Event(Event&&);
    Event(const Event&);

    Event& operator=(const Event&);
    Event& operator=(Event&&);

    AnyT& any()
    {
        return any_;
    }

    const AnyT& any() const
    {
        return any_;
    }

    bool operator==(const Event& _revt) const;

    void clear();

    Event(
        const uintptr_t    _id,
        const std::string& _rcategory)
        : pcategory_(&_rcategory)
        , id_(_id)
    {
    }

    template <class T>
    explicit Event(
        const uintptr_t    _id,
        const std::string& _rcategory,
        const T&           _rany_value)
        : pcategory_(&_rcategory)
        , id_(_id)
        , any_(_rany_value)
    {
    }

    template <class T>
    explicit Event(
        const uintptr_t    _id,
        const std::string& _rcategory,
        T&&                _uany_value)
        : pcategory_(&_rcategory)
        , id_(_id)
        , any_(std::move(_uany_value))
    {
    }

private:
    const std::string* pcategory_;
    uintptr_t          id_;
    AnyT               any_;
};

enum class GenericEvents : uintptr_t {
    Default,
    Start,
    Stop,
    Raise,
    Message,
    Timer,
    Pause,
    Resume,
    Update,
    Kill,
};

static const std::string generic_event_category("generic");

template <typename T>
inline Event make_event(const GenericEvents _id, const T& _rany_value)
{
    return Event(static_cast<uintptr_t>(_id), generic_event_category, _rany_value);
}

template <typename T>
inline Event make_event(const GenericEvents _id, T&& _uany_value)
{
    return Event(static_cast<uintptr_t>(_id), generic_event_category, std::move(_uany_value));
}

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

#ifdef USE_STD_QUEUE
using ExecQueueT = queue<ExecStub>;
#else
using ExecQueueT = Queue<ExecStub>;
#endif

int test_function_any_speed(int argc, char* argv[])
{
    cout << "sizeof(ExecStub) = " << sizeof(ExecStub) << endl;
    cout << "sizeof(Event) = " << sizeof(Event) << endl;
    cout << "sizeof(EventFunctionT) = " << sizeof(EventFunctionT) << endl;
    size_t repeat_count = 100;
    size_t insert_count = 100000;

    ExecQueueT exeq;
    uint64_t   result = 0;
    for (size_t j = 0; j < repeat_count; ++j) {
        for (size_t i = 0; i < insert_count; ++i) {
            exeq.push(
                ExecStub{
                    UniqueId{i, static_cast<UniqueT>(j)},
                    [&result, str = to_string(i)](const UniqueId& _ruid, Event&& _revent) {
                        result += _ruid.index;
#ifdef EVENT_USE_STD_ANY
                        result += *std::any_cast<size_t>(&_revent.any());
#else
                        result += *_revent.any().cast<size_t>();
#endif
                        result += str.size();
                    },
                    UniqueId{j, static_cast<UniqueT>(i)},
                    make_event(GenericEvents::Default, i)});
        }
        while (!exeq.empty()) {
            auto& rexe = exeq.front();
            rexe.exefnc(rexe.actuid, std::move(rexe.event));
            exeq.pop();
        }
    }

    cout << result << endl;

    return 0;
}

//-----------------------------------------------------------------------------

inline Event::Event()
    : pcategory_(&generic_event_category)
    , id_(static_cast<size_t>(GenericEvents::Default))
{
}

inline Event::Event(Event&& _uevt)
    : pcategory_(_uevt.pcategory_)
    , id_(_uevt.id_)
    , any_(std::move(_uevt.any_))
{
    _uevt.pcategory_ = &generic_event_category;
    _uevt.id_        = static_cast<size_t>(GenericEvents::Default);
}

inline Event::Event(const Event& _revt)
    : pcategory_(_revt.pcategory_)
    , id_(_revt.id_)
    , any_(_revt.any_)
{
}

inline Event& Event::operator=(const Event& _revt)
{
    pcategory_ = _revt.pcategory_;
    id_        = _revt.id_;
    any_       = _revt.any_;
    return *this;
}

inline Event& Event::operator=(Event&& _uevt)
{
    pcategory_       = _uevt.pcategory_;
    id_              = _uevt.id_;
    any_             = std::move(_uevt.any_);
    _uevt.pcategory_ = &generic_event_category;
    _uevt.id_        = static_cast<size_t>(GenericEvents::Default);
    return *this;
}

inline void Event::clear()
{
    pcategory_ = &generic_event_category;
    id_        = static_cast<size_t>(GenericEvents::Default);
    any_.reset();
}

inline bool Event::operator==(const Event& _revt) const
{
    return (pcategory_ == _revt.pcategory_) && (id_ == _revt.id_);
}
