# solid::frame::ipc echo tutorial

Exemplifies the use of solid_frame_ipc, solid_frame_aio and solid_frame libraries

__Source files__
 * Message definitions: [ipc_echo_messages.hpp](ipc_echo_messages.hpp)
 * The server: [ipc_echo_server.cpp](ipc_echo_server.cpp)
 * The client: [ipc_echo_client.cpp](ipc_echo_client.cpp)

 Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_ipc](../../ipc/README.md)
 
## Overview

In this tutorial you will learn how to use solid_frame_ipc library for a simple client-server application pair.
The protocol is very simple: a single message containing a string.

The client:
 * for every command line input, creates a Message and sends it to the server

The server:
 * send every message it receives, back to the sender.

You will need three source files:
 * _ipc_echo_messages.hpp_: the protocol message.
 * _ipc_echo_client.cpp_: the client implementation.
 * _ipc_echo_server.cpp_: the server implementation.
 

## Protocol definition

Then designing a client-server application, you should start with protocol definition: define the messages that will be exchanged.
For our example this is as straight forward as it can be:

```C++
#ifndef TUTORIAL_IPC_ECHO_MESSAGES_HPP
#define TUTORIAL_IPC_ECHO_MESSAGES_HPP

#include "solid/frame/ipc/ipcmessage.hpp"
#include "solid/frame/ipc/ipccontext.hpp"
#include "solid/frame/ipc/ipcprotocol_serialization_v1.hpp"

namespace ipc_echo{

struct Message: solid::frame::ipc::Message{
	std::string			str;
	
	Message(){}
	
	Message(std::string && _ustr): str(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
	}	
};

using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<0, Message>;

}//namespace ipc_echo

#endif
```

For every message of the protocol:
 * you must provide the empty constructor - it will be used on deserialization.
 * you can provide a convenient constructor for sender side initialization
 * you must provide the template serialization method.

Let us get in more details with the _serialize_ method.
The first parameter is a reference to the serialization/deserializtation engine. You will use this reference for scheduling primitive items (i.e. items that the serialization engine knows how to marshal) for marshaling.
The second parameter is a reference to a context for the currently running IPC connection.

One very important line is this one:

```C++
using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<0, Message>;
```

It helps us maintaining the same list of messages on both the client code and the server code.

The zero from the template parameter list is the protocol ID.
After that follows the list of message types. Here's an example with multiple messages:

```C++
using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<2, FirstMessage, SecondMessage, ThirdMessage>;
```

You'll see further in the client and server code how to use this ProtoSpecT type definition.

## The client implementation

First of all the client we will be implementing will be able to talk to multiple servers. Every server will be identified by it endpoint: address_name:port.
So, the client will read from standard input line by line and:
 * if the line is "q", "Q" or "quit" will exit
 * else
   * extract the first word of the line as the server endpoint
   * extract the reminder of the line as payload and create a Message with it
   * send the message to the server endpoint

Let us now walk through the code.

First off, initialize the ipc service and its prerequisites:
```C++
		AioSchedulerT			scheduler;
		
		
		frame::Manager			manager;
		frame::ipc::ServiceT	ipcservice(manager);
		
		frame::aio::Resolver	resolver;
		
		ErrorConditionT			err;
		
		err = scheduler.start(1);
		
		if(err){
			cout<<"Error starting aio scheduler: "<<err.message()<<endl;
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			cout<<"Error starting aio resolver: "<<err.message()<<endl;
			return 1;
		}
```

The scheduler is for asynchronous IO.
The manager is needed by solid frame event engine.
The resolver is needed for asynchronously resolving address names.

Next we configure the ipcservice like this:

```C++
		{
			auto 						proto = frame::ipc::serialization_v1::Protocol::create();
			frame::ipc::Configuration	cfg(scheduler, proto);
			
			ipc_echo::ProtoSpecT::setup<ipc_echo_client::MessageSetup>(*proto);
			
			cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, p.port.c_str());
			
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				return 1;
			}
		}
```

The first interesting line is:

```C++
	ipc_echo::ProtoSpecT::setup<ipc_echo_client::MessageSetup>(*proto);
```

where we make use of the ProtoSpecT type definition we've encounter in the protocol definition header.

The ipc_echo_client::MessageSetup helper structure is defined this way:

```C++
namespace ipc_echo_client{

template <class M>
void complete_message(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	if(_rerror){
		cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
		return;
	}
	
	SOLID_CHECK(_rrecv_msg_ptr and _rsent_msg_ptr);
	
	cout<<"Received from "<<_rctx.recipientName()<<": "<<_rrecv_msg_ptr->str<<endl;
}

template <typename T>
struct MessageSetup{
	void operator()(frame::ipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};


}//namespace
```
It is used by the ipc_echo::ProtoSpecT::setup function to register a message type allong with its message completion callback.

A message completion callback is called when:
 * a message failed to be sent
 * a message not needing a response was sent
 * a response was received for a message waiting for it

In our case, the message will wait for a response and the response will have the same type as the request.
 
In the "complete_message" function, we do check for error and print to standard output the name of the destination server (_rctx.recipientName()) and the message payload we got back.

Getting back to ipcservice configuration code, the next interesting line is

```C++
cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, p.port.c_str());
```

where we set an asynchronous resolver functor for converting address_names to IP addresses.

The last line from the ipcservice configuration blok we present is

```C++
cfg.connection_start_state = frame::ipc::ConnectionState::Active;
```
which sets the start state of the connection to Active.

An ipc connection can be in one of the following states:
 * Raw state: the exchange of data on connection is controlled from outside of ipc library. Will be used for implementing SOCKS5 support.
 * Passive state: the connection will send only messages specifically addressed to that connection. This is for connection initialization and authentication message exchage.
 * Active state: once the connection is initialized and authenticated, it can start sending messages from the message pool of the connection pool associated to a recipient name.

 Properly designed protocols start connection in Passive state and enter the active state when certain conditions are met.
 In our simple case, we start directly in Active state.
 
 
