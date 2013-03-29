#ifndef EXAMPLE_CONSENSUS_SERVEROBJECT_HPP
#define EXAMPLE_CONSENSUS_SERVEROBJECT_HPP

#include <string>

#include "consensus/server/consensusobject.hpp"

struct StoreRequest;
struct FetchRequest;
struct EraseRequest;


struct ServerParams: solid::consensus::server::Parameters{
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


class ServerObject: public solid::Dynamic<ServerObject, solid::consensus::server::Object>{
	typedef solid::DynamicExecuter<void, ServerObject, solid::DynamicDefaultPointerStore, int>	DynamicExecuterExT;
public:
	static void dynamicRegister();
	static void registerMessages();
	ServerObject();
	~ServerObject();
	void dynamicExecute(solid::DynamicPointer<> &_dp, int);
	
	void dynamicExecute(solid::DynamicPointer<StoreRequest> &_rsig, int);
	void dynamicExecute(solid::DynamicPointer<FetchRequest> &_rsig, int);
	void dynamicExecute(solid::DynamicPointer<EraseRequest> &_rsig, int);
private:
	/*virtual*/ void accept(solid::DynamicPointer<solid::consensus::WriteRequestMessage> &_rmsgptr);
	/*virtual*/ int recovery();
private:
	DynamicExecuterExT		exeex;
};

#endif

