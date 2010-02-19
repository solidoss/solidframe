/* Declarations file service.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TESTSERVICE_HPP
#define TESTSERVICE_HPP

#include "utility/dynamictype.hpp"
#include "foundation/service.hpp"
#include "signals.hpp"
#include "common.hpp"

struct AddrInfoIterator;
struct SocketDevice;

namespace foundation{
namespace aio{
namespace openssl{
class Context;
}
}
}

namespace concept{

class Manager;
class Visitor;

class Listener;
class Talker;


class Service: public foundation::Service{
	typedef DynamicReceiver<int, Service>	DynamicReceiverT;
public:
	enum{
		AddListener = 0,
		AddSslListener,
		AddConnection,
		AddSslConnection,
		AddTalker
	};//use with AddrInfoSignal
	
	static void dynamicRegister();
	
	Service(){}
	~Service();
	
	/*virtual*/ int signal(DynamicPointer<foundation::Signal> &_sig);
	
	int dynamicExecuteDefault(DynamicPointer<> &_dp);
	int dynamicExecute(DynamicPointer<AddrInfoSignal> &_rsig);
	
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
	
protected:
	friend class Listener;
	int insertListener(
		const AddrInfoIterator &_rai,
		bool _secure = false
	);
	virtual int insertConnection(
		const SocketDevice &_rsd,
		foundation::aio::openssl::Context *_pctx = NULL,
		bool _secure = false
	);
	
	virtual int insertTalker(
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	virtual int insertConnection(
		const AddrInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
private:
	void removeListener(Listener &);
private:
	DynamicReceiverT		dr;
};

}//namespace concept
#endif
