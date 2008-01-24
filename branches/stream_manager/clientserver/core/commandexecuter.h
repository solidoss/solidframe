#ifndef CS_COMMAND_EXECUTER_H
#define CS_COMMAND_EXECUTER_H

#include "clientserver/object.h"
#include "cmdptr.h"

namespace clientserver{

class CommandExecuter: public Object{
public:
	CommandExecuter();
	~CommandExecuter();
	int signal(CmdPtr<Command> &_cmd);
	int execute(ulong _evs, TimeSpec &_rtout);
	void mutex(Mutex *_pmut);
private:
	int execute();
	struct Data;
	Data	&d;
};

}//namespace clientserver

#endif
