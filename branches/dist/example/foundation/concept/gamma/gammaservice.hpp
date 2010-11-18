#ifndef CONCEPT_GAMMA_SERVICE_HPP
#define CONCEPT_GAMMA_SERVICE_HPP

#include "core/service.hpp"

class SocketDevice;

namespace concept{

struct AddrInfoSignal;

namespace gamma{

class Connection;

class Service: public concept::Service{
	
	typedef concept::Service	BaseT;
public:
	static concept::Service* create();
	Service();
	~Service();
	int removeConnection(Connection &);
private:
	int insertConnection(
		const SocketDevice &_rsd,
		foundation::aio::openssl::Context *_pctx,
		bool _secure
	);
};

}//namespace gamma
}//namespace concept

#endif
