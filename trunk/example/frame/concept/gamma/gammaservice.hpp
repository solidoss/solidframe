#ifndef CONCEPT_GAMMA_SERVICE_HPP
#define CONCEPT_GAMMA_SERVICE_HPP

#include "core/service.hpp"

namespace solid{
class SocketDevice;
}

namespace concept{
namespace gamma{

class Service: public solid::Dynamic<Service, concept::Service>{
public:
	Service(Manager &_rm);
	~Service();
private:
	/*virtual*/ ObjectUidT insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx,
		bool _secure
	);
};

}//namespace gamma
}//namespace concept

#endif
