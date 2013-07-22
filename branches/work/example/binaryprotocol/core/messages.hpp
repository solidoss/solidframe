#ifndef EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP
#define EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP

#include "frame/message.hpp"

struct FirstRequest: solid::Dynamic<FirstRequest, solid::frame::Message>{
};

struct SecondRequest: solid::Dynamic<SecondRequest, solid::frame::Message>{
	
};

struct FirstResponse: solid::Dynamic<FirstResponse, solid::frame::Message>{
	
};

struct SecondResponse: solid::Dynamic<SecondResponse, solid::frame::Message>{
	
};


#endif
