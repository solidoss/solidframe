#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mprpc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t                      size;
    frame::mprpc::MessageFlagsT flags;
};

InitStub initarray[] = {
    {16384000, 0}};

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

size_t connection_count(0);

std::atomic<bool>      running{true};
bool                   client_received_logout  = false;
bool                   client_received_message = false;
mutex                  mtx;
condition_variable     cnd;
frame::mprpc::Service* pmprpcclient = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message : frame::mprpc::Message {
    uint32_t     idx;
    std::string  str;
    mutable bool serialized;

    Message(uint32_t _idx)
        : idx(_idx)
        , serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
        : serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
        solid_assert(serialized || this->isBackOnSender());
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.idx, _rctx, "idx").add(_rthis.str, _rctx, "str");
        if (_s.is_serializer) {
            _rthis.serialized = true;
        }
    }

    void init()
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        str.resize(sz);
        const size_t    count        = sz / sizeof(uint64_t);
        uint64_t*       pu           = reinterpret_cast<uint64_t*>(const_cast<char*>(str.data()));
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);
        for (uint64_t i = 0; i < count; ++i) {
            pu[i] = pup[(idx + i) % pattern_size]; //pattern[i % pattern.size()];
        }
    }

    bool check() const
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        solid_dbg(generic_logger, Info, "str.size = " << str.size() << " should be equal to " << sz);
        if (sz != str.size()) {
            return false;
        }
        //return true;
        const size_t    count        = sz / sizeof(uint64_t);
        const uint64_t* pu           = reinterpret_cast<const uint64_t*>(str.data());
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);

        for (uint64_t i = 0; i < count; ++i) {
            if (pu[i] != pup[(i + idx) % pattern_size]) {
                solid_throw("Message check failed.");
                return false;
            }
        }
        return true;
    }
};

struct Logout : frame::mprpc::Message {
    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
    }
};

void client_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
    if (_rctx.isConnectionActive()) {
        //NOTE: (***) in order for client_received_logout check to work, one shoud better
        // use delayCloseConnectionPool instead of closeConnection
        // in server_complete_logout
        solid_check(client_received_message && client_received_logout);

        ++connection_count;
        running = false;
        cnd.notify_all();
    }
}

void client_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(generic_logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(generic_logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    solid_check(!_rerror);
    solid_check(_rsent_msg_ptr.get() && _rrecv_msg_ptr.get());

    if (_rrecv_msg_ptr.get()) {
        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        //cout<< _rmsgptr->str.size()<<'\n';
        transfered_size += _rrecv_msg_ptr->str.size();
        ++transfered_count;

        if (!_rrecv_msg_ptr->isBackOnSender()) {
            solid_throw("Message not back on sender!.");
        }

        {
            frame::mprpc::MessagePointerT msgptr(new Logout);

            pmprpcclient->sendMessage(
                _rctx.recipientId(), msgptr,
                {frame::mprpc::MessageFlagsE::AwaitResponse});
        }

        client_received_message = true;
    }
}

void client_complete_logout(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Logout>& _rsent_msg_ptr, std::shared_ptr<Logout>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_check(!_rerror);
    solid_check(_rsent_msg_ptr.get() && _rrecv_msg_ptr.get());
    solid_check(_rctx.service().closeConnection(_rctx.recipientId()));
    client_received_logout = true;
}

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr.get()) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId());

        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_throw("Message not on peer!.");
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_throw("Connection id should not be invalid!");
        }
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr.get()) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}

void server_complete_logout(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Logout>& _rsent_msg_ptr, std::shared_ptr<Logout>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr.get()) {
        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_throw("Message not on peer!.");
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_throw("Connection id should not be invalid!");
        }

        solid_dbg(generic_logger, Info, "send back logout");

        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }

    if (_rsent_msg_ptr.get()) {
        solid_dbg(generic_logger, Info, "close connection");
//see NOTE (***) above
//solid_check(_rctx.service().closeConnection(_rctx.recipientId()));
#if 0
        _rctx.service().delayCloseConnectionPool(
            _rctx.recipientId(),
            [](frame::mprpc::ConnectionContext &_rctx){
                solid_dbg(generic_logger, Info, "------------------");
            }
        );
#endif
    }
}

} //namespace

int test_connection_close(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EW"});

    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) && !isblank(c)) {
                pattern += static_cast<char>(c);
            }
        }
    }

    size_t sz = real_size(pattern.size());

    if (sz > pattern.size()) {
        pattern.resize(sz - sizeof(uint64_t));
    } else if (sz < pattern.size()) {
        pattern.resize(sz);
    }

    {
        AioSchedulerT sch_client;
        AioSchedulerT sch_server;

        frame::Manager m;

        CallPool<void()>       cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver   resolver(cwp);
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        ErrorConditionT        err;

        sch_client.start(1);
        sch_server.start(1);

        std::string server_port;

        { //mprpc server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_server, proto);

            proto->null(0);
            proto->registerMessage<Message>(server_complete_message, 1);
            proto->registerMessage<Logout>(server_complete_logout, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.listener_address_str = "0.0.0.0:0";

            mprpcserver.start(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mprpcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        { //mprpc client initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_client, proto);

            proto->null(0);
            proto->registerMessage<Message>(client_complete_message, 1);
            proto->registerMessage<Logout>(client_complete_logout, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &client_connection_stop;
            cfg.client.connection_start_fnc = &client_connection_start;

            cfg.pool_max_active_connection_count = 1;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

            mprpcclient.start(std::move(cfg));
        }

        pmprpcclient = &mprpcclient;

        {
            frame::mprpc::MessagePointerT msgptr(new Message(0));
            mprpcclient.sendMessage(
                "localhost", msgptr,
                initarray[0].flags | frame::mprpc::MessageFlagsE::AwaitResponse);
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        solid_dbg(generic_logger, Verbose, "stopping");
        //m.stop();
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
