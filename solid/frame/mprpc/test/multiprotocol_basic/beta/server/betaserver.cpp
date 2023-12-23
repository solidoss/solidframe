#include "betaserver.hpp"

#include "solid/frame/mprpc/mprpcservice.hpp"
#include "solid/system/log.hpp"

using namespace solid;
using namespace std;

namespace beta_server {

template <>
void complete_message<beta_protocol::FirstMessage>(
    frame::mprpc::ConnectionContext&                            _rctx,
    frame::mprpc::MessagePointerT<beta_protocol::FirstMessage>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<beta_protocol::FirstMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                                      _rerror)
{
    solid_dbg(generic_logger, Info, "");
    solid_check(!_rerror);
    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);

        ErrorConditionT err = _rctx.service().sendResponse(
            _rctx.recipientId(),
            frame::mprpc::make_message<beta_protocol::SecondMessage>(std::move(*_rrecv_msg_ptr)));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

template <>
void complete_message<beta_protocol::SecondMessage>(
    frame::mprpc::ConnectionContext&                             _rctx,
    frame::mprpc::MessagePointerT<beta_protocol::SecondMessage>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<beta_protocol::SecondMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                                       _rerror)
{
    solid_dbg(generic_logger, Info, "");
    solid_check(!_rerror);
    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

template <>
void complete_message<beta_protocol::ThirdMessage>(
    frame::mprpc::ConnectionContext&                            _rctx,
    frame::mprpc::MessagePointerT<beta_protocol::ThirdMessage>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<beta_protocol::ThirdMessage>& _rrecv_msg_ptr,
    ErrorConditionT const&                                      _rerror)
{
    solid_dbg(generic_logger, Info, "");
    solid_check(!_rerror);
    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);

        ErrorConditionT err = _rctx.service().sendResponse(
            _rctx.recipientId(),
            frame::mprpc::make_message<beta_protocol::FirstMessage>(std::move(*_rrecv_msg_ptr)));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

} // namespace beta_server
