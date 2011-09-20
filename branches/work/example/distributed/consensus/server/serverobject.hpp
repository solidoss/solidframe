#ifndef DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP
#define DISTRIBUTED_CONCEPT_SERVEROBJECT_HPP

#include <string>

#include "example/distributed/consensus/server/consensusobject.hpp"

struct StoreRequest;
struct FetchRequest;
struct EraseRequest;


struct ServerParams: consensus::Parameters{
	typedef std::vector<std::string>	StringVectorT;
	
	bool init(int _ipc_port);
	const std::string& errorString()const{
		return err;
	}
	std::ostream& print(std::ostream &_ros)const;
	StringVectorT	addrstrvec;
private:
	std::string		err;
};

std::ostream& operator<<(std::ostream &_ros, const ServerParams &_rsp);


class ServerObject: public Dynamic<ServerObject, consensus::Object>{
	typedef DynamicExecuter<void, ServerObject, int>	DynamicExecuterExT;
public:
	static void dynamicRegister();
	static void registerSignals();
	ServerObject();
	~ServerObject();
	void dynamicExecute(DynamicPointer<> &_dp, int);
	
	void dynamicExecute(DynamicPointer<StoreRequest> &_rsig, int);
	void dynamicExecute(DynamicPointer<FetchRequest> &_rsig, int);
	void dynamicExecute(DynamicPointer<EraseRequest> &_rsig, int);
private:
	/*virtual*/ void accept(DynamicPointer<consensus::RequestSignal> &_rsig);
	/*virtual*/ int recovery();
private:
	DynamicExecuterExT		exeex;
};

#endif

