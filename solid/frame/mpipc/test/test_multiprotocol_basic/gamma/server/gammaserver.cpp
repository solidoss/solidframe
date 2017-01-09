#include "../gammamessages.hpp"
#include "gammaserver.hpp"
#include "solid/serialization/typeidmap.hpp"
#include "solid/system/debug.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

using namespace solid;
using namespace std;

namespace gamma_server{

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<M> &_rsent_msg_ptr,
    std::shared_ptr<M> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
){
    idbg("");
    SOLID_CHECK(not _rerror);
    if(_rrecv_msg_ptr){
        SOLID_CHECK(not _rsent_msg_ptr);
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        if(err){
            SOLID_THROW_EX("Connection id should not be invalid!", err.message());
        }
    }
    if(_rsent_msg_ptr){
        SOLID_CHECK(not _rrecv_msg_ptr);
    }
}

template <typename T>
struct MessageSetup{
    void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
        _rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
    }
};


void register_messages(frame::mpipc::serialization_v1::Protocol &_rprotocol){
    gamma_protocol::ProtoSpecT::setup<MessageSetup>(_rprotocol);
}

}//namespace
