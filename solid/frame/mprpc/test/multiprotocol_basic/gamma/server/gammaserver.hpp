
#pragma once

#include "../../common.hpp"
#include "../gammamessages.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"
#include "solid/system/log.hpp"

namespace gamma_server {

template <class M>
void complete_message(
    solid::frame::mprpc::ConnectionContext&  _rctx,
    solid::frame::mprpc::MessagePointerT<M>& _rsent_msg_ptr,
    solid::frame::mprpc::MessagePointerT<M>& _rrecv_msg_ptr,
    solid::ErrorConditionT const&            _rerror)
{
    solid_dbg(solid::generic_logger, Info, "");
    solid_check(!_rerror);
    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);
        solid::ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

template <class Reg>
void register_messages(Reg& _rmap)
{
    auto lambda = [&]<typename T>(const TypeIdT _id, const std::string_view _name, std::type_identity<T> const& _rtype) {
        _rmap.template registerMessage<T>(_id, _name, complete_message<T>);
    };
    gamma_protocol::configure_protocol(lambda);
}
} // namespace gamma_server
