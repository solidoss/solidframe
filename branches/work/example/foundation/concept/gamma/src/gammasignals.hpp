#ifndef CONCEPT_GAMMA_SIGNALS_HPP
#define CONCEPT_GAMMA_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/aio/aiosocketpointer.hpp"

#include "utility/dynamicpointer.hpp"

namespace concept{
namespace gamma{

struct SocketData;

//!	A signal for sending a socket from a multi object to another
struct SocketMoveSignal: Dynamic<SocketMoveSignal, foundation::Signal>{
	SocketMoveSignal(const foundation::aio::SocketPointer &_rsp, SocketData *_psd): sp(_rsp), psd(_psd){}
	~SocketMoveSignal();
	foundation::aio::SocketPointer	sp;
	SocketData						*psd;
};


}//namespace gamma
}//namespace concept


#endif
