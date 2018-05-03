#include "alphaclient.hpp"
#include "../alphamessages.hpp"
#include <condition_variable>
#include <mutex>

using namespace std;
using namespace solid;

namespace alpha_client {

using IpcServicePointerT = shared_ptr<frame::mpipc::ServiceT>;
IpcServicePointerT mpipcclient_ptr;

Context* pctx;

void client_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(basic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void client_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(basic_logger, Info, _rctx.recipientId());
}

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<alpha_protocol::FirstMessage>(
    frame::mpipc::ConnectionContext&               _rctx,
    std::shared_ptr<alpha_protocol::FirstMessage>& _rsent_msg_ptr,
    std::shared_ptr<alpha_protocol::FirstMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                         _rerror)
{
    solid_dbg(basic_logger, Info, "");
    SOLID_CHECK(!_rerror);
    SOLID_CHECK(_rsent_msg_ptr && _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
    SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
    {
        lock_guard<mutex> lock(pctx->rmtx);
        --pctx->rwait_count;
        if (pctx->rwait_count == 0) {
            pctx->rcnd.notify_one();
        }
    }
}

template <>
void complete_message<alpha_protocol::SecondMessage>(
    frame::mpipc::ConnectionContext&                _rctx,
    std::shared_ptr<alpha_protocol::SecondMessage>& _rsent_msg_ptr,
    std::shared_ptr<alpha_protocol::SecondMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                          _rerror)
{
    solid_dbg(basic_logger, Info, "");
    SOLID_CHECK(!_rerror);
    SOLID_CHECK(_rsent_msg_ptr && _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
    SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
    {
        lock_guard<mutex> lock(pctx->rmtx);
        --pctx->rwait_count;
        if (pctx->rwait_count == 0) {
            pctx->rcnd.notify_one();
        }
    }
}

template <>
void complete_message<alpha_protocol::ThirdMessage>(
    frame::mpipc::ConnectionContext&               _rctx,
    std::shared_ptr<alpha_protocol::ThirdMessage>& _rsent_msg_ptr,
    std::shared_ptr<alpha_protocol::ThirdMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                         _rerror)
{
    solid_dbg(basic_logger, Info, "");
    SOLID_CHECK(!_rerror);
    SOLID_CHECK(_rsent_msg_ptr && _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
    SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
    {
        lock_guard<mutex> lock(pctx->rmtx);
        --pctx->rwait_count;
        if (pctx->rwait_count == 0) {
            pctx->rcnd.notify_one();
        }
    }
}

struct MessageSetup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, solid::TypeToType<T> _rt2t, const TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

std::string make_string(const size_t _sz)
{
    std::string         str;
    static const char   pattern[]    = "`1234567890qwertyuiop[]\\asdfghjkl;'zxcvbnm,./~!@#$%%^*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?";
    static const size_t pattern_size = sizeof(pattern) - 1;

    str.reserve(_sz);
    for (size_t i = 0; i < _sz; ++i) {
        str += pattern[i % pattern_size];
    }
    return str;
}

ErrorConditionT start(
    Context& _rctx)
{
    ErrorConditionT err;

    pctx = &_rctx;

    if (!mpipcclient_ptr) { //mpipc client initialization
        auto                        proto = ProtocolT::create();
        frame::mpipc::Configuration cfg(_rctx.rsched, proto);

        proto->null(TypeIdT(0, 0));
        alpha_protocol::protocol_setup(MessageSetup(), *proto);

        cfg.connection_stop_fnc         = &client_connection_stop;
        cfg.client.connection_start_fnc = &client_connection_start;

        cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

        cfg.pool_max_active_connection_count = _rctx.max_per_pool_connection_count;

        cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(_rctx.rresolver, _rctx.rserver_port.c_str() /*, SocketInfo::Inet4*/);

        mpipcclient_ptr = std::make_shared<frame::mpipc::ServiceT>(_rctx.rm);
        err             = mpipcclient_ptr->reconfigure(std::move(cfg));

        if (err) {
            return err;
        }

        _rctx.rwait_count += 3;

        err = mpipcclient_ptr->sendMessage(
            "localhost", std::make_shared<alpha_protocol::FirstMessage>(100000, make_string(100000)),
            {frame::mpipc::MessageFlagsE::WaitResponse});
        if (err) {
            return err;
        }
        err = mpipcclient_ptr->sendMessage(
            "localhost", std::make_shared<alpha_protocol::SecondMessage>(200000, make_string(200000)),
            {frame::mpipc::MessageFlagsE::WaitResponse});
        if (err) {
            return err;
        }
        err = mpipcclient_ptr->sendMessage(
            "localhost", std::make_shared<alpha_protocol::ThirdMessage>(30000, make_string(30000)),
            {frame::mpipc::MessageFlagsE::WaitResponse});
        if (err) {
            return err;
        }
    }

    return err;
}

void stop()
{
    mpipcclient_ptr.reset();
}

} // namespace alpha_client
