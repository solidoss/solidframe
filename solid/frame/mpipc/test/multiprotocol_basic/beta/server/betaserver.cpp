#include "betaserver.hpp"
#include "../betamessages.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/serialization/v1/typeidmap.hpp"
#include "solid/system/debug.hpp"
#include "solid/utility/dynamicpointer.hpp"

using namespace solid;
using namespace std;

namespace beta_server {

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<beta_protocol::FirstMessage>(
    frame::mpipc::ConnectionContext&              _rctx,
    std::shared_ptr<beta_protocol::FirstMessage>& _rsent_msg_ptr,
    std::shared_ptr<beta_protocol::FirstMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                        _rerror)
{
    idbg("");
    SOLID_CHECK(!_rerror);
    if (_rrecv_msg_ptr) {
        SOLID_CHECK(!_rsent_msg_ptr);

        ErrorConditionT err = _rctx.service().sendResponse(
            _rctx.recipientId(),
            std::make_shared<beta_protocol::SecondMessage>(std::move(*_rrecv_msg_ptr)));

        SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        SOLID_CHECK(!_rrecv_msg_ptr);
    }
}

template <>
void complete_message<beta_protocol::SecondMessage>(
    frame::mpipc::ConnectionContext&               _rctx,
    std::shared_ptr<beta_protocol::SecondMessage>& _rsent_msg_ptr,
    std::shared_ptr<beta_protocol::SecondMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                         _rerror)
{
    idbg("");
    SOLID_CHECK(!_rerror);
    if (_rrecv_msg_ptr) {
        SOLID_CHECK(!_rsent_msg_ptr);
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        SOLID_CHECK(!_rrecv_msg_ptr);
    }
}

template <>
void complete_message<beta_protocol::ThirdMessage>(
    frame::mpipc::ConnectionContext&              _rctx,
    std::shared_ptr<beta_protocol::ThirdMessage>& _rsent_msg_ptr,
    std::shared_ptr<beta_protocol::ThirdMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                        _rerror)
{
    idbg("");
    SOLID_CHECK(!_rerror);
    if (_rrecv_msg_ptr) {
        SOLID_CHECK(!_rsent_msg_ptr);

        ErrorConditionT err = _rctx.service().sendResponse(
            _rctx.recipientId(),
            std::make_shared<beta_protocol::FirstMessage>(std::move(*_rrecv_msg_ptr)));

        SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        SOLID_CHECK(!_rrecv_msg_ptr);
    }
}

struct MessageSetup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, solid::TypeToType<T> _rt2t, const TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void register_messages(ProtocolT& _rprotocol)
{
    beta_protocol::protocol_setup(MessageSetup(), _rprotocol);
}
} // namespace beta_server
