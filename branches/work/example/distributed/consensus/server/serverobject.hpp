#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include <deque>
#include <queue>

#include "system/common.hpp"

#ifdef HAVE_UNORDERED_MAP
#include <unordered_map>
#include <unordered_set>
#else
#include <map>
#endif

#include "system/timespec.hpp"

#include "foundation/object.hpp"
#include "foundation/signal.hpp"
#include "utility/stack.hpp"
#include "example/distributed/consensus/core/signalidentifier.hpp"

struct C;
struct StoreSignal;
struct FetchSignal;
struct EraseSignal;
struct ConceptSignal;
struct ConceptSignalIdetifier;

class ServerObject: public Dynamic<ServerObject, foundation::Object>{
	typedef DynamicExecuter<void, ServerObject>			DynamicExecuterT;
	typedef DynamicExecuter<void, ServerObject, int>	DynamicExecuterExT;
public:
	static void dynamicRegister();
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp);
	
	void dynamicExecute(DynamicPointer<ConceptSignal> &_rsig);
	
	void dynamicExecute(DynamicPointer<> &_dp, int);
	
	void dynamicExecute(DynamicPointer<StoreSignal> &_rsig, int);
	void dynamicExecute(DynamicPointer<FetchSignal> &_rsig, int);
	void dynamicExecute(DynamicPointer<EraseSignal> &_rsig, int);
	
	
	int execute(ulong _sig, TimeSpec &_tout);
private:
	struct ClientRequest;
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	size_t insertClientRequest(DynamicPointer<ConceptSignal> &_rsig);
	void eraseClientRequest(size_t _idx);
	ClientRequest& clientRequest(size_t _idx);
	const ClientRequest& clientRequest(size_t _idx)const;
	bool checkAlreadyReceived(DynamicPointer<ConceptSignal> &_rsig);
	void registerClientRequestTimer(const TimeSpec &_rts, size_t _idx);
	size_t popClientRequestTimeout(const TimeSpec &_rts);
	void scheduleNextClientRequestTimer(TimeSpec &_rts);
private:
	struct ClientRequest{
		ClientRequest():uid(0), state(0){}
		ClientRequest(DynamicPointer<ConceptSignal>	&_rsig):sig(_rsig), uid(0), state(0){}
		DynamicPointer<ConceptSignal>	sig;
		uint16							uid;
		uint16							timerid;
		uint16							state;
	};
	struct TimerData{
		TimerData(
			const TimeSpec&_rts,
			size_t _idx = -1,
			uint16 _timerid = 0
		): timepos(_rts), idx(_idx), timerid(_timerid){}
		TimeSpec	timepos;
		size_t		idx;
		uint16		timerid;
	};
	struct ReqCmpEqual{
		bool operator()(const ConceptSignalIdetifier* const & _req1, const ConceptSignalIdetifier* const & _req2)const;
	};
	struct ReqCmpLess{
		bool operator()(const ConceptSignalIdetifier* const & _req1, const ConceptSignalIdetifier* const & _req2)const;
	};
	struct ReqHash{
		size_t operator()(const ConceptSignalIdetifier* const & _req1)const;
	};
	struct SenderCmpEqual{
		bool operator()(const ConceptSignalIdetifier & _req1, const ConceptSignalIdetifier& _req2)const;
	};
	struct SenderCmpLess{
		bool operator()(const ConceptSignalIdetifier& _req1, const ConceptSignalIdetifier& _req2)const;
	};
	struct SenderHash{
		size_t operator()(const ConceptSignalIdetifier& _req1)const;
	};
	struct TimerDataCmp{
		bool operator()(const TimerData &_rtd1, const TimerData &_rtd2)const;
	};
	typedef std::deque<ClientRequest>	ClientRequestVectorT;
#ifdef HAVE_UNORDERED_MAP
	typedef std::unordered_map<const ConceptSignalIdetifier*, size_t, ReqHash, ReqCmpEqual>	ClientRequestMapT;
	typedef std::unordered_set<ConceptSignalIdetifier, SenderHash, SenderCmpEqual>				SenderSetT;
#else
	typedef std::map<const ConceptSignalIdetifier*, size_t, ReqCmpLess>						ClientRequestMapT;
	typedef std::set<ConceptSignalIdetifier, SenderCmpLess>									SenderSetT;
#endif
	typedef Stack<size_t>				SizeTStackT;
	typedef std::priority_queue<TimerData, std::vector<TimerData>, TimerDataCmp>				TimerPriorityQueueT;
private:
	DynamicExecuterT		exe;
	DynamicExecuterExT		exeex;
	uint32					consensusval;
	
	
	uint32					crtval;
	
	ClientRequestMapT		reqmap;
	ClientRequestVectorT	reqvec;
	SizeTStackT				freeposstk;
	SenderSetT				senderset;
	TimerPriorityQueueT		timerpriorq;
};

#endif

