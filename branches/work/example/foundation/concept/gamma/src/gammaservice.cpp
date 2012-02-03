#include "system/debug.hpp"
#include "system/mutex.hpp"

#include "algorithm/serialization/binary.hpp"

#include "foundation/objectpointer.hpp"
#include "foundation/aio/openssl/opensslsocket.hpp"

#include "core/listener.hpp"
#include "core/manager.hpp"

#include "gamma/gammaservice.hpp"

#include "gammaconnection.hpp"

namespace fdt=foundation;

namespace concept{
namespace gamma{
#ifdef HAVE_SAFE_STATIC
struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

/*static*/ concept::gamma::Service* Service::create(){
	static InitServiceOnce	init(m());
	return new Service();
}
#else
void once_init(){
	Connection::initStatic(_rm);
}
/*static*/ concept::gamma::Service* Service::create(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_init, once);
	return new Service();
}
#endif
Service::Service(){
}

Service::~Service(){
}

bool Service::insertConnection(
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){
	//create a new connection with the given channel
	fdt::ObjectPointer<Connection> conptr(new Connection(_rsd));
	this->insert<AioSchedulerT>(conptr, 0);
	return true;
}

void Service::eraseObject(const Connection &_ro){
	ObjectUidT objuid(_ro.uid());
	idbg("gamma "<<fdt::Manager::the().computeServiceId(objuid.first)<<' '<<fdt::Manager::the().computeIndex(objuid.first)<<' '<<objuid.second);
}

}//namespace gamma
}//namespace concept

