# solid::frame::mpipc echo tutorial

Exemplifies the use of solid_frame_mpipc, solid_frame_aio and solid_frame libraries

__Source files__
 * Message definitions: [mpipc_echo_messages.hpp](mpipc_echo_messages.hpp)
 * The server: [mpipc_echo_server.cpp](mpipc_echo_server.cpp)
 * The client: [mpipc_echo_client.cpp](mpipc_echo_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mpipc](../../solid/frame/mpipc/README.md)
 
## Overview

In this tutorial you will learn how to use solid_frame_mpipc library for a basic client-server application pair.
The client and server will exchange a simple message consisting of a single string.

**The client**:
 * for every command line input
   * extract the recipient endpoint
   * extract payload
   * creates a Message with the payload and sends it to the server at recipient endpoint

**The server**:
 * send every message it receives, back to the sender.

You will need three source files:
 * _mpipc_echo_messages.hpp_: the protocol message.
 * _mpipc_echo_client.cpp_: the client implementation.
 * _mpipc_echo_server.cpp_: the server implementation.
 

## Protocol definition

When designing a client-server application, you should start with protocol definition - i.e. define the messages that will be exchanged.
For our example this is quite straight forward:

```C++
#ifndef TUTORIAL_MPIPC_ECHO_MESSAGES_HPP
#define TUTORIAL_MPIPC_ECHO_MESSAGES_HPP

#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

namespace ipc_echo{

struct Message: solid::frame::mpipc::Message{
    std::string         str;
    
    Message(){}
    
    Message(std::string && _ustr): str(std::move(_ustr)){}
    
    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(str, "str");
    }   
};

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, Message>;

}//namespace ipc_echo

#endif
```

For every message of the protocol:
 * you must provide the empty constructor - it will be used on deserialization.
 * you can provide a convenient constructor for convenient sender side initialization.
 * you must provide the template serialization method.

Let us get in more details with the _serialize_ method.
The first parameter is a reference to the serialization/deserializtation engine. You will use this reference for scheduling primitive items (i.e. items that the serialization engine knows how to marshal) for marshaling.
The second parameter is a reference to a context for the currently running MPIPC connection.

One very important line is this one:

```C++
using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, Message>;
```

It helps us maintaining the same list of messages on both the client code and the server code.

The zero from the template parameter list is the protocol ID.
Afterwards, follows the list of message types. Here's an example with multiple messages:

```C++
using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<2, FirstMessage, SecondMessage, ThirdMessage>;
```

You'll see further in the client and server code how to use this ProtoSpecT type definition.

## The client implementation

First of all the client we will be implementing will be able to talk to multiple servers. Every server will be identified by its endpoint: address_name:port.
So, the client will read from standard input line by line and:
 * if the line is "q", "Q" or "quit" will exit
 * else
   * extract the first word of the line as the server endpoint
   * extract the reminder of the line as payload and create a Message with it
   * send the message to the server endpoint

Let us now walk through the code.

First off, initialize the mpipc service and its prerequisites:

```C++
        AioSchedulerT           scheduler;
        
        
        frame::Manager          manager;
        frame::mpipc::ServiceT  ipcservice(manager);
        
        frame::aio::Resolver    resolver;
        
        ErrorConditionT         err;
        
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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, proto);
            
            ipc_echo::ProtoSpecT::setup<ipc_echo_client::MessageSetup>(*proto);
            
            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());
            
            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
            
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
    frame::mpipc::ConnectionContext &_rctx,
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
    void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
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
 
In the "complete_message" function, we check for error and print to standard output the name of the destination server (_rctx.recipientName()) and the message payload we got back.

Getting back to ipcservice configuration code, the next interesting line is

```C++
cfg.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());
```

where we set an asynchronous resolver functor for converting address_names to IP addresses.

The last line from the ipcservice configuration block we present is

```C++
cfg.connection_start_state = frame::mpipc::ConnectionState::Active;
```
which sets the start state of the connection to Active.

An mpipc connection can be in one of the following states:
 * Raw state: the exchange of data on connection is controlled from outside of mpipc library. Will be used for implementing SOCKS5 support.
 * Passive state: the connection will send only messages specifically addressed to that connection. This is for connection initialization and authentication message exchange.
 * Active state: once the connection is initialized and authenticated, it can start sending messages from the message pool of the connection pool associated to a recipient name.

Properly designed protocols start connection in Passive state and enter the active state when certain conditions are met.
In our simple case, we start directly in Active state.

Now that we've completed configuring and starting the ipcservice, let see how the command loop looks like:
```C++
        while(true){
            string  line;
            getline(cin, line);
            
            if(line == "q" or line == "Q" or line == "quit"){
                break;
            }
            {
                string      recipient;
                size_t      offset = line.find(' ');
                
                if(offset != string::npos){
                    recipient = line.substr(0, offset);
                    ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), 0|frame::mpipc::MessageFlags::WaitResponse);
                }else{
                    cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
                }
            }
        }
```
 
The interesting line in the above block is
```C++
    ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), 0|frame::mpipc::MessageFlags::WaitResponse);
```
Which schedules the message to be sent to a server identified by recipient address.
As you can see in the above line, the message we are sending is a request for which we expect a response. The response will come on the catch-all message completion callback ipc_echo_client::complete_message that we've set up earlier.

The mpipc::Sevice class contains mutiple methods for sending messages.
In the [MPIPC Request](../mpipc_request) tutorial you will learn about other method for sending a message - called _sendRequest_ - which sends a message for which a response is expected. The completion callback given to _sendRequest_ will be called with the received response.


### Compile

```bash
$ cd solid_frame_tutorials/mpipc_echo
$ c++ -o mpipc_echo_client mpipc_echo_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The server implementation

The code of the server is simpler than, and quite similar to, the client one.
E.g. the initialization of the ipcservice and its prerequisites is the same as on the client. Different is, offcourse, the configuration, which is done like this:
```C++
        {
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, proto);
            
            ipc_echo::ProtoSpecT::setup<ipc_echo_server::MessageSetup>(*proto);
            
            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;
            
            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
            
            err = ipcsvc.reconfigure(std::move(cfg));
            
            if(err){
                cout<<"Error starting ipcservice: "<<err.message()<<endl;
                manager.stop();
                return 1;
            }
            {
                std::ostringstream oss;
                oss<<ipcsvc.configuration().server.listenerPort();
                cout<<"server listens on port: "<<oss.str()<<endl;
            }
        }
```

The first interesting part is the one setting up the protocol - which also is similar to the client code:

```C++
    ipc_echo::ProtoSpecT::setup<ipc_echo_server::MessageSetup>(*proto);
```

The implementation for ipc_echo_server::MessageSetup being:

```C++
namespace ipc_echo_server{

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<M> &_rsent_msg_ptr,
    std::shared_ptr<M> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
){
    SOLID_CHECK(not _rerror);
    
    if(_rrecv_msg_ptr){
        SOLID_CHECK(not _rsent_msg_ptr);
        SOLID_CHECK_ERROR(_rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr)));
    }
    
    if(_rsent_msg_ptr){
        SOLID_CHECK(not _rrecv_msg_ptr);
    }
}

template <typename T>
struct MessageSetup{
    void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
        _rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
    }
};


}//namespace
```

You can see in the above code the callback for handling messages on the server.
It is a single callback, called on two situations:
 * When a message is received - in which case _rrecv_msg_ptr is not empty
 * When a response was sent back - in which case _rsent_msg_ptr is not empty.

Also please note the main action of the callback:
```C++
    ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
```

_rctx.recipientId() will return the identifier of the current connection on which we've received the message and we're using it to send back the received message as response. Note that we do not allocate a new message for response but use the already allocated one.

Back to the ipcservice configuration block, the next interesting code is:

```C++
            cfg.listener_address_str = p.listener_addr;
            cfg.listener_address_str += ':';
            cfg.listener_address_str += p.listener_port;
```

which configures the service listener to listen on certain address (usually "0.0.0.0") and a certain port.

The last interesting code chunk from the configuration block, which actually is called after the configuration terminates, is:

```C++
            {
                std::ostringstream oss;
                oss<<ipcservice.configuration().listenerPort();
                cout<<"server listens on port: "<<oss.str()<<endl;
            }
```
which prints on standard-output the actual port used for listening. This is particularly useful when the ipcservice is initialized with a '0' (zero) listener port, in which case the system allocates an empty port for you.

The last code block for the server is one which keeps the server alive until user input:
```C++
    cout<<"Press any char and ENTER to stop: ";
    char c;
    cin>>c;
```

### Compile

```bash
$ cd solid_frame_tutorials/mpipc_echo
$ c++ -o mpipc_echo_server mpipc_echo_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have two applications a client and a server let us test it in a little scenario with two servers and a client.

**Console-1**:
```BASH
$ ./mpipc_echo_server 0.0.0.0:3333
```
**Console-2**:
```BASH
$ ./mpipc_echo_client
localhost:3333 Some text sent to the first server
localhost:4444 Some text sent to the second server
```
**Console-3**:
```BASH
#wait for a while
$ ./mpipc_echo_server 0.0.0.0:4444
```

On the client you will see that the text is immediately received back from :3333 server while the second text is received back only after the second server is started. This is because, normally, the ipcservice will try re-sending the message until the recipient side becomes available. Use **mpipc::MessageFlags::OneShotSend** to change the behavior and only try once to send the message and immediately fail if the server is offline.

## Next

Now that you have some idea about the solid_frame_mpipc library and you are still interested of its capabilities you can further check the next tutorials:

 * [MPIPC Request](../mpipc_request)
 * [MPIPC File](../mpipc_file)
