#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/actor.hpp"
#include "solid/frame/reactor.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"
#include "solid/utility/string.hpp"

#include <functional>
#include <future>
#include <iostream>
#include <signal.h>
#include <sstream>

using namespace std;
using namespace solid;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
using SchedulerT    = frame::Scheduler<frame::Reactor>;
using AtomicSizeT   = atomic<size_t>;

namespace {

const solid::LoggerT logger("test");

using CreateTupleT   = std::tuple<SchedulerT&, frame::ServiceT&, size_t>;
using RequestTupleT  = std::tuple<frame::ActorIdT, size_t>;
using ResponseTupleT = std::tuple<size_t>;

class Connection final : public frame::aio::Actor {
public:
    Connection(
        const frame::ActorIdT& _acc_id, size_t _repeat_count, size_t _device_id,
        ::AtomicSizeT& _rconnection_count,
        promise<void>& _rprom)
        : acc_id_(_acc_id)
        , repeat_count_(_repeat_count)
        , device_id_(_device_id)
        , rconnectio_count_(_rconnection_count)
        , rprom_(_rprom)
    {
    }
    ~Connection() override {}
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;

private:
    const frame::ActorIdT acc_id_;
    size_t                repeat_count_;
    size_t                device_id_;
    size_t                value_ = 0;
    ::AtomicSizeT&        rconnectio_count_;
    promise<void>&        rprom_;
};

class Device final : public frame::Actor {
public:
    void onEvent(frame::ReactorContext& _rctx, Event&& _revent) override;

private:
    size_t value_ = 0;
};

class Account final : public frame::Actor {
public:
    void onEvent(frame::ReactorContext& _rctx, Event&& _revent) override;

private:
    vector<frame::ActorIdT> device_vec_;
};

void Account::onEvent(frame::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(logger, Info, "account " << this << " event: " << _revent);

    if (_revent == generic_event_start) {
    } else if (_revent == generic_event_resume) {
        CreateTupleT* pt = _revent.any().cast<CreateTupleT>();
        solid_check(pt != nullptr);
        SchedulerT&      device_scheduler     = std::get<0>(*pt);
        frame::ServiceT& device_service       = std::get<1>(*pt);
        size_t           account_device_count = std::get<2>(*pt);
        ErrorConditionT  err;

        solid_dbg(logger, Info, "Create " << account_device_count << " devices");

        for (size_t i = 0; i < account_device_count; ++i) {
            device_vec_.emplace_back(device_scheduler.startActor(make_shared<Device>(), device_service, make_event(GenericEvents::Start), err));
        }
    } else if (generic_event_kill == _revent) {
        solid_dbg(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        RequestTupleT* pt = _revent.any().cast<RequestTupleT>();
        solid_check(pt != nullptr);
        frame::ActorIdT con_id    = std::get<0>(*pt);
        size_t          device_id = std::get<1>(*pt);
        solid_dbg(logger, Verbose, "actor " << this << " con_id = " << con_id << " device: " << device_id);
        _rctx.manager().notify(device_vec_[device_id % device_vec_.size()], make_event(GenericEvents::Message, RequestTupleT(*pt)));
    }
}

void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    const frame::ActorIdT my_id = _rctx.service().id(*this);
    solid_dbg(logger, Info, "connection " << this << " " << my_id << " event: " << _revent);

    if (_revent == generic_event_resume) {
        _rctx.manager().notify(acc_id_, make_event(GenericEvents::Message, RequestTupleT(make_tuple(my_id, device_id_))));
        ++device_id_;
    } else if (generic_event_kill == _revent) {
        solid_dbg(generic_logger, Error, this << "ignore kill event");
    } else if (generic_event_message == _revent) {
        ResponseTupleT* pt = _revent.any().cast<ResponseTupleT>();
        solid_check(pt != nullptr);
        size_t value = std::get<0>(*pt);
        value_ += value;

        solid_dbg(logger, Verbose, this << " " << my_id << " recv value = " << value << " repeat_count = " << repeat_count_);

        --repeat_count_;
        if (repeat_count_ != 0) {
            _rctx.manager().notify(acc_id_, make_event(GenericEvents::Message, RequestTupleT(make_tuple(my_id, device_id_))));
            ++device_id_;
        } else {
            postStop(_rctx);
            if (rconnectio_count_.fetch_sub(1) == 1) {
                rprom_.set_value();
            }
        }
    }
}

void Device::onEvent(frame::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(logger, Info, "device " << this << " event: " << _revent);

    if (generic_event_kill == _revent) {
        solid_dbg(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        RequestTupleT* pt = _revent.any().cast<RequestTupleT>();
        solid_check(pt != nullptr);
        frame::ActorIdT con_id = std::get<0>(*pt);
        solid_dbg(logger, Verbose, "device " << this << " con_id = " << con_id);
        _rctx.manager().notify(con_id, make_event(GenericEvents::Message, ResponseTupleT(value_)));
        ++value_;
    }
}

} //namespace

int test_event_stress(int argc, char* argv[])
{
    size_t account_count            = 10000;
    size_t account_connection_count = 10;
    size_t account_device_count     = 20;
    size_t repeat_count             = 40;
    int    wait_seconds             = 10000;

    if (argc > 1) {
        repeat_count = make_number(argv[1]);
    }

    if (argc > 2) {
        account_count = make_number(argv[2]);
    }

    if (argc > 3) {
        account_connection_count = make_number(argv[3]);
    }

    if (argc > 4) {
        account_device_count = make_number(argv[4]);
    }

    solid::log_start(std::cerr, {"test:EWX"});

    auto lambda = [&]() {
        ErrorConditionT err;
        AioSchedulerT   connection_scheduler;
        SchedulerT      account_scheduler;
        SchedulerT      device_scheduler;
        frame::Manager  manager;
        frame::ServiceT account_service{manager};
        frame::ServiceT device_service{manager};
        frame::ServiceT connection_service{manager}; //should stop first
        AtomicSizeT     connection_count(0);
        promise<void>   prom;

        connection_scheduler.start(1);
        account_scheduler.start(1);
        device_scheduler.start(1);

        for (size_t i = 0; i < account_count; ++i) {
            const auto acc_id = account_scheduler.startActor(make_shared<Account>(), account_service, make_event(GenericEvents::Start), err);
            for (size_t j = 0; j < account_connection_count; ++j) {
                ++connection_count;
                connection_scheduler.startActor(make_shared<Connection>(acc_id, repeat_count, j, connection_count, prom), connection_service, make_event(GenericEvents::Start), err);
            }
        }

        account_service.notifyAll(make_event(GenericEvents::Resume, CreateTupleT(make_tuple(std::ref(device_scheduler), std::ref(device_service), account_device_count))));

        connection_service.notifyAll(make_event(GenericEvents::Resume));

        solid_check(prom.get_future().wait_for(chrono::seconds(wait_seconds)) == future_status::ready);
    };

    if (async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }

    return 0;
}
