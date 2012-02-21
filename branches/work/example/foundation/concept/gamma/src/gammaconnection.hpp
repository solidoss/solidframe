#ifndef CONCEPT_GAMMA_CONNECTION_HPP
#define CONCEPT_GAMMA_CONNECTION_HPP


#include "utility/dynamictype.hpp"
#include "utility/streampointer.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "core/tstring.hpp"
#include "core/common.hpp"

#include "gammareader.hpp"
#include "gammawriter.hpp"

#include "foundation/aio/aiomultiobject.hpp"

#include <deque>

class SocketAddress;

namespace foundation{

class Visitor;

}

class InputStream;
class OutputStream;
class InputOutputStream;

namespace concept{

class Visitor;
class Manager;

//signals
struct InputStreamSignal;
struct OutputStreamSignal;
struct InputOutputStreamSignal;
struct StreamErrorSignal;


namespace gamma{

class Service;
class Command;

//signals:
struct SocketMoveSignal;

class Logger: public protocol::Logger{
protected:
	virtual void doInFlush(const char*, unsigned);
	virtual void doOutFlush(const char*, unsigned);
};

struct SocketData{
	SocketData(uint _sid):w(_sid), r(_sid, w), /*evs(0),*/ pcmd(NULL){
	}
	Writer		w;
	Reader		r;
	//ulong		evs;
	Command		*pcmd;
};

//! Alpha connection implementing the alpha protocol resembling somehow the IMAP protocol
/*!
	It uses a reader and a writer to implement a state machine for the 
	protocol communication. 
*/
class Connection: public Dynamic<Connection, foundation::aio::MultiObject>{
	typedef DynamicExecuter<void, Connection>	DynamicExecuterT;
public:
	enum{
		SocketRegister,
		SocketInit,
		SocketBanner,
		SocketParsePrepare,
		SocketParse,
		SocketIdleParse,
		SocketIdleWait,
		SocketIdleDone,
		SocketExecutePrepare,
		SocketExecute,
		SocketParseWait,
		SocketUnregister,
		SocketLeave
	};
	
	typedef Service	ServiceT;
	
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	
	static Connection &the();
	
	Connection(const SocketDevice &_rsd);
	
	~Connection();
	
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	
	//! The implementation of the protocol's state machine
	/*!
		The method will be called within a foundation::SelectPool by an
		foundation::aio::Selector.
	*/
	int execute(ulong _sig, TimeSpec &_tout);
	
	//! creator method for new commands
	Command* create(const String& _name, Reader &_rr);
	
	//! Generate a new request id.
	/*!
		Every request has an associated id.
		We want to prevent receiving unexpected responses.
		
		The commands that are not responses - like those received in idle state, come with
		the request id equal to zero so this must be skiped.
	*/
	uint32 newRequestId(int _pos = -1);
	bool   isRequestIdExpected(uint32 _v, int &_rpos)const;
	void   deleteRequestId(uint32 _v);
	
	SocketData &socketData(const uint _sid);
	
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<InputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<OutputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<InputOutputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<StreamErrorSignal> &_rsig);
	void dynamicExecute(DynamicPointer<SocketMoveSignal> &_rsig);
	
	void appendContextString(std::string &_str);
private:
	int executeSocket(const uint _sid, const TimeSpec &_tout);
	
	void prepareReader(const uint _sid);
	
	Command* doCreateSlave(const String& _name, Reader &_rr);
	Command* doCreateMaster(const String& _name, Reader &_rr);
	
	static void doInitStaticSlave(Manager &_rm);
	static void doInitStaticMaster(Manager &_rm);
	
	void pushExecQueue(uint _sid, uint32 _evs);
	uint popExecQueue();
	
	int doSocketPrepareBanner(const uint _sid, SocketData &_rsd);
	void doSocketPrepareParse(const uint _sid, SocketData &_rsd);
	int doSocketParse(const uint _sid, SocketData &_rsd, const bool _isidle = false);
	int doSocketExecute(const uint _sid, SocketData &_rsd, const int _state = 0);
	void doSendSocketSignal(const uint _sid);

private:
	typedef std::vector<SocketData*>				SocketDataVectorT;
	typedef std::vector<std::pair<uint32, int> >	RequestIdVectorT;
	struct StreamData{
		StreamPointer<InputStream>	pis;
		StreamPointer<OutputStream>	pos;
		StreamPointer<InputOutputStream>	pios;
	};
	typedef std::deque<StreamData>					StreamDataVectorT;
	typedef Stack<uint32>							UIntStackT;

private:
	bool				isslave;
	uint32				crtreqid;
	DynamicExecuterT	dr;
	SocketDataVectorT	sdv;
	RequestIdVectorT	ridv;
	StreamDataVectorT	streamv;
	UIntStackT			freestk;
};



inline SocketData& Connection::socketData(uint _sid){
	return *sdv[_sid];
}

}//namespace gamma
}//namespace concept

#endif
