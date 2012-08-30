#ifndef CONCEPT_GAMMA_SERVICE_HPP
#define CONCEPT_GAMMA_SERVICE_HPP

#include "core/service.hpp"

class SocketDevice;

namespace concept{

namespace gamma{

class Connection;

class Service: public Dynamic<Service, concept::Service>{
public:
	static concept::gamma::Service* create();
	Service();
	~Service();
	void eraseObject(const Connection &);
private:
	ObjectUidT insertConnection(
		const SocketDevice &_rsd,
		foundation::aio::openssl::Context *_pctx,
		bool _secure
	);
};

}//namespace gamma
}//namespace concept

#endif
