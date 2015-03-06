#include "system/debug.hpp"
#include "system/mutex.hpp"

#include "serialization/binary.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "core/manager.hpp"

#include "gamma/gammaservice.hpp"

#include "gammaconnection.hpp"

using namespace solid;

namespace concept{
namespace gamma{
	
#ifdef HAS_SAFE_STATIC
struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

Service::Service(Manager &_rm):BaseT(_rm){
	static InitServiceOnce	init(_rm);
}
#else
void once_init(){
	Connection::initStatic(Manager::the());
}
Service::Service(Manager &_rm):BaseT(_rm){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_init, once);
}
#endif

Service::~Service(){
}

ObjectUidT Service::insertConnection(
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	//create a new connection with the given channel
	DynamicPointer<Connection>			conptr(new Connection(_rsd));
	ObjectUidT rv = registerObject(*conptr);
	DynamicPointer<frame::aio::Object>	objptr(conptr);
	Manager::the().scheduleAioObject(objptr);
	return rv;
}

}//namespace gamma
}//namespace concept

