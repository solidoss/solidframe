#ifndef CONCEPT_GAMMA_SIGNALS_HPP
#define CONCEPT_GAMMA_SIGNALS_HPP

#include "frame/message.hpp"
#include "frame/aio/aiosocketpointer.hpp"

#include "utility/dynamicpointer.hpp"

namespace concept{
namespace gamma{

struct SocketData;

//!	A signal for sending a socket from a multi object to another
struct SocketMoveMessage: solid::Dynamic<SocketMoveMessage, solid::frame::Message>{
	SocketMoveMessage(const solid::frame::aio::SocketPointer &_rsp, SocketData *_psd): sp(_rsp), psd(_psd){}
    virtual ~SocketMoveMessage();
    solid::frame::aio::SocketPointer	sp;
	SocketData							*psd;
};


}//namespace gamma
}//namespace concept


#endif
