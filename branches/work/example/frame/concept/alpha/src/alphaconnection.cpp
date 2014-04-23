// alphaconnection.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "system/debug.hpp"
#include "system/timespec.hpp"
#include "system/mutex.hpp"
#include "system/socketaddress.hpp"

#include "utility/ostream.hpp"
#include "utility/istream.hpp"
#include "utility/iostream.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/requestuid.hpp"


#include "core/manager.hpp"
#include "core/messages.hpp"

#include "alpha/alphaservice.hpp"

#include "alphaconnection.hpp"
#include "alphacommand.hpp"
#include "alphamessages.hpp"
#include "alphaprotocolfilters.hpp"
#include "audit/log.hpp"


using namespace solid;

namespace concept{
namespace alpha{

void Logger::doInFlush(const char *_pb, unsigned _bl){
	if(Log::the().isSet(Log::any, Log::Input)){
		Log::the().record(Log::Input, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::the().done();
	}
}

void Logger::doOutFlush(const char *_pb, unsigned _bl){
	if(Log::the().isSet(Log::any, Log::Output)){
		Log::the().record(Log::Output, Log::any, 0, __FILE__, __FUNCTION__, __LINE__).write(_pb, _bl);
		Log::the().done();
	}
}

/*static*/ void Connection::initStatic(Manager &_rm){
	Command::initStatic(_rm);
	dynamicRegister();
}

/*static*/ Connection::DynamicMapperT		Connection::dm;

/*static*/ void Connection::dynamicRegister(){
	dm.insert<FilePointerMessage, Connection>();
	dm.insert<RemoteListMessage, Connection>();
	dm.insert<FetchSlaveMessage, Connection>();
}

#ifdef UDEBUG
/*static*/ Connection::ConnectionsVectorT& Connection::connections(){
	static ConnectionsVectorT cv;
	return cv;
}
#endif


Connection::Connection(
	ResolveData &_rai
):	wtr(&logger), rdr(wtr, &logger),
	pcmd(NULL), ai(_rai), reqid(1){
	aiit = ai.begin();
	state(Connect);
#ifdef UDEBUG
	connections().push_back(this);
#endif
}

Connection::Connection(
	const SocketDevice &_rsd
):	BaseT(_rsd), wtr(&logger),rdr(wtr, &logger),
	pcmd(NULL), reqid(1){

	state(Init);
#ifdef UDEBUG
	connections().push_back(this);
#endif

}


/*
NOTE: 
* Releasing the connection here and not in a base class destructor because
here we know the exact type of the object - so the service can do other things 
based on the type.
* Also it ensures a proper/safe visiting. Suppose the unregister would have taken 
place in a base destructor. If the visited object is a leaf, one may visit
destroyed data.
NOTE: 
* Visitable data must be destroyed after releasing the connection!!!
*/

Connection::~Connection(){
	idbg("destroy connection id "<<this->id()<<" pcmd "<<pcmd);
	delete pcmd; pcmd = NULL;
#ifdef UDEBUG
	for(
		ConnectionsVectorT::iterator it(connections().begin());
		it != connections().end();
		++it
	){
		if(*it == this){
			*it = connections().back();
			connections().pop_back();
			break;
		}
	}
#endif
}


/*virtual*/ bool Connection::notify(DynamicPointer<frame::Message> &_rmsgptr){
	if(this->state() < 0){
		_rmsgptr.clear();
		return false;//no reason to raise the pool thread!!
	}
	dv.push_back(DynamicPointer<>(_rmsgptr));
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}


/*
	The main loop with the implementation of the alpha protocol's
	state machine. We dont need a loop, we use the ConnectionSelector's
	loop - returning OK.
	The state machine is a simple switch
*/

/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	concept::Manager &rm = concept::Manager::the();
	frame::requestuidptr->set(this->id(), Manager::the().id(*this).second);
	//_tout.add(2400);
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventTimeout)){
		if(state() == ConnectWait){
			state(Connect);
		}else
			if(_rexectx.eventMask() & frame::EventTimeout){
				edbg("timeout occured - destroy connection "<<state());
			}else{
				edbg("error occured - destroy connection "<<state());
			}
			_rexectx.close();
			return;
	}
	
	if(notified()){//we've received a signal
		DynamicHandler<DynamicMapperT>	dh(dm);
		ulong 							sm(0);
		{
			Locker<Mutex>	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL){
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(dv.begin(), dv.end());
				dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_rexectx.eventMask()) return;
	}
	const uint32 sevs = socketEventsGrab();
	if(sevs & frame::EventDoneError){
		_rexectx.close();
		return;
	}
// 	if(socketEvents() & frame::OUTDONE){
// 		switch(state()){
// 			case IdleExecute:
// 			case ExecuteTout:
// 				state(Execute);
// 				break;
// 			case Execute:
// 				break;
// 			case Connect:
// 				state(Init);
// 				break;
// 			default:
// 				state(Parse);
// 		}
// 	}
	int rc;
	switch(state()){
		case Init:
			wtr.buffer(protocol::text::SpecificBuffer(2*1024));
			rdr.buffer(protocol::text::SpecificBuffer(2*1024));
			if(this->socketIsSecure()){
				frame::aio::AsyncE rv = this->socketSecureAccept();
				state(Banner);
				switch(rv){
					case frame::aio::AsyncSuccess:
						_rexectx.reschedule();
					case frame::aio::AsyncWait:
						break;
					case frame::aio::AsyncError:
						_rexectx.close();
				}
				return;
			}else{
				state(Banner);
			}
		case Banner:{
			concept::Manager	&rm = concept::Manager::the();
			uint32				myport(rm.ipc().basePort());
			IndexT				objid(this->id());
			uint32				objuid(Manager::the().id(*this).second);
			char				host[SocketInfo::HostStringCapacity];
			char				port[SocketInfo::ServiceStringCapacity];
			SocketAddress		addr;
			
			
			
			writer()<<"* Hello from alpha server ("<<myport<<' '<<' '<< objid<<' '<<objuid<<") [";
			socketLocalAddress(addr);
			addr.toString(
				host,
				SocketInfo::HostStringCapacity,
				port,
				SocketInfo::ServiceStringCapacity,
				SocketInfo::NumericService | SocketInfo::NumericHost
			);
			writer()<<host<<':'<<port<<" -> ";
			socketRemoteAddress(addr);
			addr.toString(
				host,
				SocketInfo::HostStringCapacity,
				port,
				SocketInfo::ServiceStringCapacity,
				SocketInfo::NumericService | SocketInfo::NumericHost
			);
			writer()<<host<<':'<<port<<"]"<<'\r'<<'\n';
			writer().push(&Writer::flushAll);
			state(Execute);
			}break;
		case ParsePrepare:
			idbg("PrepareReader");
			prepareReader();
		case Parse:
			//idbg("Parse");
			state(Parse);
			switch((rc = reader().run())){
				case Reader::Success: break;
				case Reader::Wait:
					if(socketHasPendingSend()){
						socketTimeoutSend(3000);
					}else if(socketHasPendingRecv()){
						socketTimeoutRecv(3000);
					}else{
						_rexectx.waitFor(TimeSpec(2000));
					}
					state(ParseTout);
					return;
				case Reader::Failure:
					edbg("");
					_rexectx.close();
					return;
				case Reader::Yield:
					_rexectx.reschedule();
					return;
			}
			if(reader().isError()){
				delete pcmd; pcmd = NULL;
				logger.inFlush();
				state(Execute);
				writer().push(Writer::putStatus);
				break;
			}
		case ExecutePrepare:
			logger.inFlush();
			idbg("PrepareExecute");
			pcmd->execute(*this);
			state(Execute);
		case Execute:
			//idbg("Execute");
			switch((rc = writer().run())){
				case Writer::Wait:
					if(socketHasPendingSend()){
						socketTimeoutSend(3000);
						state(ExecuteIOTout);
					}/*else if(socketHasPendingRecv()){
						socketTimeoutRecv(3000);
						state(ExecuteIOTout);
					}*/else{
						//_tout.add(2000);
						_rexectx.waitFor(TimeSpec(2000));
						state(ExecuteTout);
					}
					return;
				case Writer::Success:
					if(state() != IdleExecute){
						delete pcmd; pcmd = NULL;
						state(ParsePrepare);
					}else{
						state(Parse);
					}
				case Writer::Yield:
					_rexectx.reschedule();
					return;
				case Writer::Failure:
					_rexectx.close();
					return;
				default:
					edbg("rc = "<<rc);
					cassert(false);
					return;
			}
			break;
		case IdleExecute:
			//idbg("IdleExecute");
			if(sevs & frame::EventDoneSend){
				state(Execute);
				break;
			}return;
		case Connect:
			switch(socketConnect(aiit)){
				case frame::aio::AsyncError:
					if(++aiit){
						state(Connect);
						break;
					}
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:
					idbg("");
					state(Init);
					//idbg("");
					return;//for register
				case frame::aio::AsyncWait:
					state(ConnectWait);
					idbg("");
					return;
			}
			break;
		case ConnectWait:
			state(Init);
			//delete(paddr); paddr = NULL;
			break;
		case ParseTout:
			if(sevs & frame::EventDoneRecv){
				state(Parse);
				break;
			}
			return;
		case ExecuteIOTout:
			idbg("State: ExecuteTout ");
			if(sevs & frame::EventDoneSend){
				state(Execute);
				break;
			}
		case ExecuteTout:
			return;
	}
	_rexectx.reschedule();
}

//prepare the reader and the writer for a new command
void Connection::prepareReader(){
	writer().clear();
	reader().clear();
	//logger.outFlush();
	reader().push(&Reader::checkChar, protocol::text::Parameter('\n'));
	reader().push(&Reader::checkChar, protocol::text::Parameter('\r'));
	reader().push(&Reader::fetchKey<Reader, Connection, AtomFilter, Command>, protocol::text::Parameter(this, 64));
	reader().push(&Reader::checkChar, protocol::text::Parameter(' '));
	reader().push(&Reader::fetchFilteredString<TagFilter>, protocol::text::Parameter(&writer().tag(),64));
}

void Connection::dynamicHandle(DynamicPointer<> &_dp){
	wdbg("Received unknown signal on ipcservice");
}

void Connection::dynamicHandle(DynamicPointer<RemoteListMessage> &_rmsgptr){
	idbg("");
	if(_rmsgptr->requid && _rmsgptr->requid != reqid){
		idbg("RemoteListMessage signal rejected");
		return;
	}
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv = pcmd->receiveMessage(_rmsgptr);
		switch(rv){
			case AsyncError:
				idbg("");
				break;
			case AsyncSuccess:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case AsyncWait:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicHandle(DynamicPointer<FetchSlaveMessage> &_rmsgptr){
	idbg("");
	if(_rmsgptr->requid && _rmsgptr->requid != reqid) return;
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		int rv = pcmd->receiveMessage(_rmsgptr);
		switch(rv){
			case AsyncError:
				idbg("");
				break;
			case AsyncSuccess:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					state(Execute);
				}
				break;
			case AsyncWait:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}
void Connection::dynamicHandle(DynamicPointer<FilePointerMessage> &_rmsgptr){
	idbg("");
	if(_rmsgptr->reqidx && _rmsgptr->reqidx != reqid){
		return;
	}
	idbg("");
	newRequestId();//prevent multiple responses with the same id
	if(pcmd){
		switch(pcmd->receiveFilePointer(*_rmsgptr)){
			case AsyncError:
				idbg("");
				break;
			case AsyncSuccess:
				idbg("");
				if(state() == ParseTout){
					state(Parse);
				}
				if(state() == ExecuteTout){
					idbg("");
					state(Execute);
				}
				break;
			case AsyncWait:
				idbg("");
				state(IdleExecute);
				break;
		}
	}
}

}//namespace alpha
}//namespace concept
