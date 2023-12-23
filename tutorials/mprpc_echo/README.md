# solid::frame::mprpc echo tutorial

Exemplifies the use of solid_frame_mprpc, solid_frame_aio and solid_frame libraries

__Source files__
 * Message definitions: [mprpc_echo_messages.hpp](mprpc_echo_messages.hpp)
 * The server: [mprpc_echo_server.cpp](mprpc_echo_server.cpp)
 * The client: [mprpc_echo_client.cpp](mprpc_echo_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous actor model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mprpc](../../solid/frame/mprpc/README.md)

## Overview

In this tutorial you will learn how to use solid_frame_mprpc library for a basic client-server application pair.
The client and server will exchange a simple message consisting of a single string.

**The client**:
 * for every command line input
   * extract the recipient endpoint
   * extract payload
   * creates a Message with the payload and sends it to the server at recipient endpoint

**The server**:
 * send every message it receives, back to the sender.

You will need three source files:
 * _mprpc_echo_messages.hpp_: the protocol message.
 * _mprpc_echo_client.cpp_: the client implementation.
 * _mprpc_echo_server.cpp_: the server implementation.


## Protocol definition

When designing a client-server application, you should start with protocol definition - i.e. define the messages that will be exchanged.
For our example this is quite straight forward:

```C++
#pragma once

#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/system/common.hpp"

namespace ipc_echo{

struct Message: solid::frame::mprpc::Message{
    std::string         str;

    Message(){}

    Message(std::string && _ustr): str(std::move(_ustr)){}

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str");
    }
};

using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<Message>(), 1);
}

}//namespace ipc_echo

#endif
```

For every message of the protocol:
 * you must provide the empty constructor - it will be used on deserialization.
 * you can provide a convenient constructor for convenient sender side initialization.
 * you must provide the SOLID_PROTOCOL_V2 method.

Let us get in more details with the SOLID_PROTOCOL_V2 method:
 * The first parameter is a reference to the serialization/deserializtation engine.
 * The second parameter is a reference to "this" object - it is a const reference in case of serialization and non-const in case of deserialization.
 * The third parameter is a reference to context.
 * The fourth parameter is "const char *" containing the name of the object in upper level - for now the name is only used for debugging.

The following code:

```C++
using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<uint8_t>;

template <class Stub>
inline void protocol_setup(Stub _s, ProtocolT& _rproto)
{
    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _s(_rproto, solid::TypeToType<Message>(), 1);
}
```

is the actual definition of the protocol - i.e. the messages it is composed of. __protocol_setup__ does not do the actual message registration - it is done by the given Stub.

You'll see further in the client and server code how is __protocol_setup__ being used.

## The client implementation

First of all the client we will be implementing will be able to talk to multiple servers. Every server will be identified by its endpoint: address_name:port.
So, the client will read from standard input line by line and:
 * if the line is "q", "Q" or "quit" will exit
 * else
   * extract the first word of the line as the server endpoint
   * extract the reminder of the line as payload and create a Message with it
   * send the message to the server endpoint

Let us now walk through the code.

First off, initialize the mprpc service and its prerequisites:

```C++
AioSchedulerT           scheduler;


frame::Manager          manager;
frame::mprpc::ServiceT  ipcservice(manager);

CallPool<void()>        cwp{ThreadPoolConfiguration(1)};
frame::aio::Resolver    resolver(cwp);

ErrorConditionT         err;

err = scheduler.start(1);

if(err){
    cout<<"Error starting aio scheduler: "<<err.message()<<endl;
    return 1;
}

```

The scheduler is for asynchronous IO.
The manager is needed by solid frame event engine.
The resolver is needed for asynchronously resolving address names.

Next we configure the ipcservice like this:

```C++
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(scheduler, proto);

    ipc_echo::protocol_setup(ipc_echo_client::MessageSetup(), *proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());

    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

    err = ipcservice.start(std::move(cfg));

    if(err){
        cout<<"Error starting ipcservice: "<<err.message()<<endl;
        return 1;
    }
}
```

The first line of interest is:

```C++
ipc_echo::protocol_setup(ipc_echo_client::MessageSetup(), *proto);
```

where we call ipc_echo::protocol_setup we've encounter in the protocol definition header using ipc_echo_client::MessageSetup structure from below:

```C++
namespace ipc_echo_client{

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    frame::mprpc::MessagePointerT<M>&              _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    if (_rerror) {
        cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
        return;
    }

    solid_check(_rrecv_msg_ptr and _rsent_msg_ptr);

    cout << "Received from " << _rctx.recipientName() << ": " << _rrecv_msg_ptr->str << endl;
}

struct MessageSetup {
    void operator()(ipc_echo::ProtocolT& _rprotocol, TypeToType<ipc_echo::Message> _t2t, const ipc_echo::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<ipc_echo::Message>(complete_message<ipc_echo::Message>, _rtid);
    }
};

}//namespace
```

The MessageSetup::operator() will register a message onto protocol, with its completion callback.

A message completion callback is called when:
 * a message failed to be sent
 * a message not needing a response was sent
 * a response was received for a message waiting for it

In our case, the message will wait for a response and the response will have the same type as the request.

In the "complete_message" function, we check for error and print to standard output the name of the destination server (_rctx.recipientName()) and the message payload we got back.

Getting back to ipcservice configuration code, the next interesting line is

```C++
cfg.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());
```

where we set an asynchronous resolver functor for converting address_names to IP addresses.

The last line from the ipcservice configuration block we present is

```C++
cfg.connection_start_state = frame::mprpc::ConnectionState::Active;
```
which sets the start state of the connection to Active.

An mprpc connection can be in one of the following states:
 * Raw state: the exchange of data on connection is controlled from outside of mprpc library. Will be used for implementing SOCKS5 support.
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
            ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), 0|frame::mprpc::MessageFlags::WaitResponse);
        }else{
            cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
        }
    }
}
```

The interesting line in the above block is
```C++
ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), 0|frame::mprpc::MessageFlags::WaitResponse);
```
Which schedules the message to be sent to a server identified by recipient address.
As you can see in the above line, the message we are sending is a request for which we expect a response. The response will come on the catch-all message completion callback ipc_echo_client::complete_message that we've set up earlier.

The mprpc::Sevice class contains mutiple methods for sending messages.
In the [MPRPC Request](../mprpc_request) tutorial you will learn about other method for sending a message - called _sendRequest_ - which sends a message for which a response is expected. The completion callback given to _sendRequest_ will be called with the received response.


### Compile

```bash
$ cd solid_frame_tutorials/mprpc_echo
$ c++ -o mprpc_echo_client mprpc_echo_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mprpc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The server implementation

The code of the server is simpler than, and quite similar to, the client one.
E.g. the initialization of the ipcservice and its prerequisites is the same as on the client. Different is, offcourse, the configuration, which is done like this:
```C++
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(scheduler, proto);

    ipc_echo::protocol_setup(ipc_echo_server::MessageSetup(), *proto);

    cfg.server.listener_address_str = p.listener_addr;
    cfg.server.listener_address_str += ':';
    cfg.server.listener_address_str += p.listener_port;

    cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

    err = ipcsvc.start(std::move(cfg));

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
ipc_echo::protocol_setup(ipc_echo_server::MessageSetup(), *proto);
```

The implementation for ipc_echo_server::MessageSetup being:

```C++
namespace ipc_echo_server{

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    frame::mprpc::MessagePointerT<M>&              _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(not _rerror);

    if (_rrecv_msg_ptr) {
        solid_check(not _rsent_msg_ptr);
        solid_check(not _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr)));
    }

    if (_rsent_msg_ptr) {
        solid_check(not _rrecv_msg_ptr);
    }
}

struct MessageSetup {
    void operator()(ipc_echo::ProtocolT& _rprotocol, TypeToType<ipc_echo::Message> _t2t, const ipc_echo::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<ipc_echo::Message>(complete_message<ipc_echo::Message>, _rtid);
    }
};

}//namespace
```

You can see in the above code the callback for handling messages on the server.
It is a single callback, called on two situations:
 * When a message is received - in which case _rrecv_msg_ptr is not empty
 * When a response was sent back - in which case _rsent_msg_ptr is not empty.

Also, please note the main action of the callback:
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
cout<<"Press ENTER to stop: ";
cin.ignore();
```

### Compile

```bash
$ cd solid_frame_tutorials/mprpc_echo
$ c++ -o mprpc_echo_server mprpc_echo_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mprpc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have the client and the server applications, let us test them in a little scenario with two servers and a client.

**Console-1**:
```BASH
$ ./mprpc_echo_server 0.0.0.0:3333
```
**Console-2**:
```BASH
$ ./mprpc_echo_client
localhost:3333 Some text sent to the first server
localhost:4444 Some text sent to the second server
```
**Console-3**:
```BASH
#wait for a while
$ ./mprpc_echo_server 0.0.0.0:4444
```

On the client you will see that the text is immediately received back from :3333 server while the second text is received back only after the second server is started. This is because, normally, the ipcservice will try re-sending the message until the recipient side becomes available. Use **mprpc::MessageFlags::OneShotSend** to change the behavior and only try once to send the message and immediately fail if the server is offline.

## Next

Now that you have some idea about the solid_frame_mprpc library and you are still interested of its capabilities you can further check the next tutorial on solid_frame_mprpc library:

 * [MPRPC Request](../mprpc_request)
