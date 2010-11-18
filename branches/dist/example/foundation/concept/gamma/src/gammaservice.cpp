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

struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

/*static*/ concept::Service* Service::create(){
	//TODO: staticproblem
	static InitServiceOnce	init(Manager::the());
	return new Service();
}

Service::Service(){
}

Service::~Service(){
}

int Service::insertConnection(
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){
	//create a new connection with the given channel
	Connection *pcon = new Connection(_rsd);
	if(_pctx){
		//TODO:
		//pcon->socketSecureSocket(_pctx->createSocket());
	}
	//register it into the service
	if(this->insert(*pcon, this->index())){
		delete pcon;
		return BAD;
	}
	// add it into a connection pool
	Manager::the().pushJob(static_cast<fdt::aio::Object*>(pcon));
	return OK;
}

int Service::removeConnection(Connection &_rcon){
	//TODO:
	//unregisters the connection from the service.
	this->remove(_rcon);
	return OK;
}

}//namespace gamma
}//namespace concept

