# solid::frame::mprpc relay echo tutorial

Exemplifies the use of solid_frame_mprpc for relaying messages between connections.

__Source files__
 * Register message definition: [mprpc_echo_relay_register.hpp](mprpc_echo_relay_register.hpp)
 * The relay server: [mprpc_echo_relay_server.cpp](mprpc_echo_relay_server.cpp)
 * The client: [mprpc_echo_relay_client.cpp](mprpc_echo_relay_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous actor model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mprpc](../../solid/frame/mprpc/README.md)
 * read the [informations about solid_frame_mprpc relay engine](../../solid/frame/mprpc/README.md#relay_engine)

## Overview

In this tutorial you will learn how to use solid_frame_mprpc library for peer-to-peer message passing through a relay server.
The client will send a message to a peer specified by its relay name. 

**The client**:
 * keeps a registered active connection to the relay server
 * for every command line input
   * extract the peer name
   * extract payload
   * creates a Message with the payload and sends it to the server to be relayed to the peer
   * upon receiving a Message from a peer, prints it to console and echoes it back to the sending peer as response.

**The relay server**:
 * relays messages from registered connections.

You will need three source files:
 * _mprpc_echo_relay_register.hpp_: connection register protocol message.
 * _mprpc_echo_relay_client.cpp_: the client implementation.
 * _mprpc_echo_relay_relay.cpp_: the relay server implementation.


## Protocol definition

There are two protocols involved in this tutorial environment:
 * the one between clients and the relay server, used for registering connections onto relay server: Register message
 * the one between clients themselves: Message.

We put the definition of the Register message in a separate header(mprpc_echo_relay_register.hpp) used by both the client and server code:
```C++
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"

struct Register : solid::frame::mprpc::Message {
    std::string name;

    Register() {}

    Register(const std::string& _name)
        : name(_name)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.name, _rctx, "name");
    }
};

using TypeIdT = std::pair<uint8_t, uint8_t>;

namespace std {
//inject a hash for TypeIdT
template <>
struct hash<TypeIdT> {
    typedef TypeIdT     argument_type;
    typedef std::size_t result_type;
    result_type         operator()(argument_type const& s) const noexcept
    {
        result_type const h1(std::hash<uint8_t>{}(s.first));
        result_type const h2(std::hash<uint>{}(s.second));
        return h1 ^ (h2 << 1); // or use boost::hash_combine (see Discussion)
    }
};
} //namespace std

using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<TypeIdT>;

constexpr TypeIdT null_type_id{0, 0};
constexpr TypeIdT register_type_id{0, 1};
```

The Message is only used by the client so it will be defined in the mprpc_echo_relay_client.cpp as follows:
```C++
struct Message : solid::frame::mprpc::Message {
    std::string name;
    std::string data;

    Message() {}

    Message(const std::string& _name, std::string&& _ustr)
        : name(_name)
        , data(std::move(_ustr))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.name, _rctx, "name");
        _s.add(_rthis.data, _rctx, "data");
    }
};
```

## The client implementation

We will skip the instantiation of needed objects (Manager, mprpc::Service, Scheduler, Resolver) as they were presented in previous tutorials. Now we will continue with the configuration of mprpc::Service.

First initializing the protocol:
```C++
            auto   proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(scheduler, proto);

            proto->null(null_type_id);
            proto->registerMessage<Register>(con_register, register_type_id);
            proto->registerMessage<Message>(on_message, TypeIdT{1, 1});
```

where con_register is a lambda defined as follows:
```C++
            auto con_register = [](
                frame::mprpc::ConnectionContext& _rctx,
                frame::mprpc::MessagePointerT<Register>&       _rsent_msg_ptr,
                frame::mprpc::MessagePointerT<Register>&       _rrecv_msg_ptr,
                ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);

                if (_rrecv_msg_ptr and _rrecv_msg_ptr->name.empty()) {
                    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
                        idbg("peerb --- enter active error: " << _rerror.message());
                        return frame::mprpc::MessagePointerT();
                    };
                    cout << "Connection registered" << endl;
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
                }
            };
```

The above lambda is called when receiving the Register response message from the server. It activates the connection (connectionNotifyEnterActiveState(...)) when the server responds with success (the name field of the Register structure is empty).

The "on_message" in protocol initialization code snippet is also a lambda, defined as follows:

```C++
            auto on_message = [&p](
                frame::mprpc::ConnectionContext& _rctx,
                frame::mprpc::MessagePointerT<Message>&        _rsent_msg_ptr,
                frame::mprpc::MessagePointerT<Message>&        _rrecv_msg_ptr,
                ErrorConditionT const&           _rerror) {

                if (_rrecv_msg_ptr) {
                    cout << _rrecv_msg_ptr->name << ": " << _rrecv_msg_ptr->data << endl;
                    if (!_rsent_msg_ptr) {
                        //we're on peer - echo back the response
                        _rrecv_msg_ptr->name = p.name;
                        ErrorConditionT err  = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                        (void)err;
                    }
                }
            };
```

The captured "p" variable is a data structure with parsed program parameters.

The lambda is called after:
 * receiving a Message from a peer client
 * receiving a response Message back on sender
 * sending back a response Message

Of interest are only the first two situations:
 * on both cases we output to console the message name and data
 * and for the first case, we send back the response.

In order to be able to receive messages from other peers, a client must always stay connected and registered to the server. To activate this behaviour on client, we need to:
 * register the connection after it was established
 * create a connection pool with active connections

To register connection after it was established we need to configure mprpc::Service with a callback for connection start event:

```C++
    cfg.client.connection_start_fnc = on_connection_start;
```
where, _on_connection_start_ is a lambda:

```C++
            auto on_connection_start = [&p](frame::mprpc::ConnectionContext& _rctx) {
                idbg(_rctx.recipientId());

                auto            msgptr = std::make_shared<Register>(p.name);
                ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mprpc::MessageFlagsE::WaitResponse});
                solid_check(not err, "failed send Register");
            };
```
which instantiates a Register request message and sends it to the server.

To activate a connection pool directed to the server, we need to call the following method after having successfully configured the mprpc::Service:

```C++
    ipcservice.createConnectionPool(p.server_addr.c_str());
```

Next we have the loop for reading user input, construct the Message and send it to the server to be relayed to the requested peer:

```C++
        while (true) {
            string line;
            getline(cin, line);

            if (line == "q" or line == "Q" or line == "quit") {
                break;
            }
            {
                string recipient;
                size_t offset = line.find(' ');
                if (offset != string::npos) {
                    recipient = p.server_addr + '/' + line.substr(0, offset);
                    ipcservice.sendMessage(recipient.c_str(), make_shared<Message>(p.name, line.substr(offset + 1)), {frame::mprpc::MessageFlagsE::WaitResponse});
                } else {
                    cout << "No recipient name specified. E.g:" << endl
                         << "alpha Some text to send" << endl;
                }
            }
        }
```

Notable is that now we specify the recipient name of the form "relay_host_addr/peer_name" (e.g. "localhost/alpha", "localhost:4345/alpha"). The _relay_host_addr_ is the address of the relay server while _peer_name_ must be a name registered on the relay server.

### Compile

```bash
$ cd solid_frame_tutorials/mprpc_echo_relay
$ c++ -o mprpc_echo_relay_client mprpc_echo_relay_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mprpc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The relay server implementation

The relay server, listens on a specified port for connections. On a newly created connection, a Register request is received with a given name. The server uses that name to register the connection onto the RelayEngine.

The program will expect none or a single parameter - the address on which the server should listen (default: 0.0.0.0:3333) - and will initialize the following structure:

```C++
struct Parameters {
    Parameters()
        : listener_port("3333")
        , listener_addr("0.0.0.0")
    {
    }

    string listener_port;
    string listener_addr;
};
```
The main method starts by parsing the program arguments:

```C++
int main(int argc, char* argv[])
{
    Parameters p;

    if (!parseArguments(p, argc, argv))
        return 0;

```

then, (preferable in a new block), instantiate the needed objects (notable being the relay_engine) and tries to start the scheduler:

```C++
    {
        AioSchedulerT                         scheduler;
        frame::Manager                        manager;
        frame::mprpc::relay::SingleNameEngine relay_engine(manager); //before relay service because it must outlive it
        frame::mprpc::ServiceT                ipcservice(manager);
        ErrorConditionT                       err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }
```

next follows a new block for configuring the mprpc::Service:

```C++
        {
            auto con_register = [&relay_engine](
                frame::mprpc::ConnectionContext& _rctx,
                frame::mprpc::MessagePointerT<Register>&       _rsent_msg_ptr,
                frame::mprpc::MessagePointerT<Register>&       _rrecv_msg_ptr,
                ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);
                if (_rrecv_msg_ptr) {
                    solid_check(!_rsent_msg_ptr);
                    idbg("recv register request: " << _rrecv_msg_ptr->name);

                    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->name));

                    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                    solid_check(!err, "Failed sending register response: " << err.message());

                } else {
                    solid_check(!_rrecv_msg_ptr);
                    idbg("sent register response");
                }
            };

            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(scheduler, relay_engine, proto);

            proto->null(null_type_id);
            proto->registerMessage<Register>(con_register, register_type_id);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;
            cfg.relay_enabled = true;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            err = ipcservice.start(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                manager.stop();
                return 1;
            }
            {
                std::ostringstream oss;
                oss << ipcservice.configuration().server.listenerPort();
                cout << "server listens on port: " << oss.str() << endl;
            }
        }
```

the line:

```C++
    proto->registerMessage<Register>(con_register, register_type_id);
```

registers the con_register lambda to be called after a Register request message is received or response was sent back to the client.

All that the con_register lambda does, upon receiving a request, is to register the current, calling connection, onto relay_engine:

```C++
    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->name));
```

and send back the response

```C++
    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

    solid_check(!err, "Failed sending register response: " << err.message());
```

going back to the configuration code, after configuring the listener, we enable the relay onto the server:

```C++
    cfg.relay_enabled = true;
```
and set Active as the start state for connections:

```C++
    cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;
```

the last part of the configuration block handles the service _start_ call and, in case of success, it prints the port on which the server is listening on.

If everything was Ok, the server awaits for the user to press ENTER, and then exits:

```C++
    cout << "Press ENTER to stop: ";
        cin.ignore();
    }//service instantiation block
    return 0;
}//main function block
```

### Compile

```bash
$ cd solid_frame_tutorials/mprpc_echo_relay
$ c++ -o mprpc_echo_relay_relay mprpc_echo_relay_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mprpc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have both the client and the relay server applications, let us test them in a little scenario with two clients and a server.

**Console-1**:
```BASH
$ ./mprpc_echo_relay_server
```
**Console-2**:
```BASH
$ ./mprpc_echo_relay_client alpha
beta Some text sent from alpha to beta
beta Some other text sent from alpha to beta
```
**Console-3**:
```BASH
$ ./mprpc_echo_relay_client beta
alpha Some text sent from beta to alpha
alpha Some other text sent from beta to alpha
```

## Next

Well, this is the last tutorial for now.
If solid_frame_mprpc library or solid_frame has captured your attention, start using it and/or give feedback.

