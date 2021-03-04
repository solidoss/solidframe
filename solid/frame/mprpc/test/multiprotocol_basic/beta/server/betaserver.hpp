
#pragma once

#include "../../common.hpp"
#include "../betamessages.hpp"
namespace beta_server {

template <class M>
void complete_message(
    solid::frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&                     _rsent_msg_ptr,
    std::shared_ptr<M>&                     _rrecv_msg_ptr,
    solid::ErrorConditionT const&           _rerror);

template <class Reg>
void register_messages(Reg& _rmap)
{
    auto lambda = [&](const TypeIdT _id, const std::string_view _name, auto const& _rtype) {
        using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
        _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
    };
    beta_protocol::configure_protocol(lambda);
}

} // namespace beta_server
