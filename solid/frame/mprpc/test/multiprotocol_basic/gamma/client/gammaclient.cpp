#include "gammaclient.hpp"
#include "../gammamessages.hpp"

using namespace std;
using namespace solid;

namespace gamma_client {

using IpcServicePointerT = shared_ptr<frame::mprpc::ServiceT>;
IpcServicePointerT mprpcclient_ptr;

Context* pctx;

void client_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void client_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_dbg(generic_logger, Info, "");
    solid_check(!_rerror);
    solid_check(_rsent_msg_ptr && _rrecv_msg_ptr);
    solid_check(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
    solid_check(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
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

    if (!mprpcclient_ptr) { //mprpc client initialization
        auto                        proto = ProtocolT::create();
        frame::mprpc::Configuration cfg(_rctx.rsched, proto);

        proto->null(TypeIdT(0, 0));
        gamma_protocol::protocol_setup(MessageSetup(), *proto);

        cfg.connection_stop_fnc         = &client_connection_stop;
        cfg.client.connection_start_fnc = &client_connection_start;

        cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

        cfg.pool_max_active_connection_count = _rctx.max_per_pool_connection_count;

        cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rctx.rresolver, _rctx.rserver_port.c_str() /*, SocketInfo::Inet4*/);

        mprpcclient_ptr = std::make_shared<frame::mprpc::ServiceT>(_rctx.rm);
        mprpcclient_ptr->start(std::move(cfg));

        _rctx.rwait_count += 3;

        err = mprpcclient_ptr->sendMessage(
            "localhost", std::make_shared<gamma_protocol::FirstMessage>(100000, make_string(100000)),
            {frame::mprpc::MessageFlagsE::AwaitResponse});
        if (err) {
            return err;
        }
        err = mprpcclient_ptr->sendMessage(
            "localhost", std::make_shared<gamma_protocol::SecondMessage>(200000, make_string(200000)),
            {frame::mprpc::MessageFlagsE::AwaitResponse});
        if (err) {
            return err;
        }
        err = mprpcclient_ptr->sendMessage(
            "localhost", std::make_shared<gamma_protocol::ThirdMessage>(30000, make_string(30000)),
            {frame::mprpc::MessageFlagsE::AwaitResponse});
        if (err) {
            return err;
        }
    }
    return err;
}

void stop()
{
    mprpcclient_ptr.reset();
}

} // namespace gamma_client
