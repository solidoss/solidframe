
#pragma once

#include "../../common.hpp"
#include "../alphamessages.hpp"

namespace alpha_server {
template <class M>
void complete_message(
    solid::frame::mprpc::ConnectionContext&  _rctx,
    solid::frame::mprpc::MessagePointerT<M>& _rsent_msg_ptr,
    solid::frame::mprpc::MessagePointerT<M>& _rrecv_msg_ptr,
    solid::ErrorConditionT const&            _rerror);

template <class Reg>
void register_messages(Reg& _rmap)
{
    auto lambda = [&]<typename T>(const TypeIdT _id, const std::string_view _name, std::type_identity<T> const& _rtype) {
        _rmap.template registerMessage<T>(_id, _name, complete_message<T>);
    };
    alpha_protocol::configure_protocol(lambda);
}

} // namespace alpha_server
