#ifndef CONCEPT_GAMMA_COMMAND_HPP
#define CONCEPT_GAMMA_COMMAND_HPP

#include "utility/streampointer.hpp"
#include "system/specific.hpp"

#include "core/common.hpp"
#include "core/tstring.hpp"

namespace solid{
class InputStream;
class OutputStream;
}

using solid::int64;

namespace concept{

class Manager;

namespace gamma{

class Connection;
class Reader;

//! A base class for all alpha protocol commands
class Command: public solid::SpecificObject{
protected:
	Command();
public:
	//! Initiate static data for all commands - like registering serializable structures.
	static void initStatic(Manager &_rm);
	//! Virtual destructor
	virtual ~Command();
	//! Called by the reader to learn how to parse the command
	virtual void initReader(Reader &) = 0;
	//! Called by alpha::Connection to prepare the response
	virtual void execute(const uint _sid) = 0;
	
	virtual void contextData(ObjectUidT &_robjuid);

};

}//namespace gamma
}//namespace concept


#endif
