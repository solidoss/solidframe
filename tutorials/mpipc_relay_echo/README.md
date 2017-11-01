# solid::frame::mpipc relay echo tutorial

Exemplifies the use of solid_frame_mpipc for relaying messages between connections.

__Source files__
 * Register message definition: [mpipc_relay_echo_register.hpp](mpipc_relay_echo_register.hpp)
 * The relay server: [mpipc_relay_echo_server.cpp](mpipc_relay_echo_server.cpp)
 * The client: [mpipc_relay_echo_client.cpp](mpipc_relay_echo_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mpipc](../../solid/frame/mpipc/README.md)
 * read the [informations about solid_frame_mpipc relay engine](../../solid/frame/mpipc/README.md#relay_engine)

## Overview

In this tutorial you will learn how to use solid_frame_mpipc library for peer-to-peer message passing through a relay server.
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
 * _mpipc_relay_echo_register.hpp_: connection register protocol message.
 * _mpipc_relay_echo_client.cpp_: the client implementation.
 * _mpipc_relay_echo_relay.cpp_: the relay server implementation.


## Protocol definition

There are two protocols involved in this tutorial environment:
 * the one between clients and the relay server, used for registering connections onto relay server: Register message
 * the one between clients themselves: Message.

We put the definition of the Register message in a separate header(mpipc_relay_echo_register.hpp) used by both the client and server code:
```C++
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

struct Register : solid::frame::mpipc::Message {
    std::string name;

    Register() {}

    Register(const std::string& _name)
        : name(_name)
    {
    }

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(name, "name");
    }
};

```

The Message is only used by the client so it will be defined in the mpipc_relay_echo_client.cpp as follows:
```C++
struct Message : solid::frame::mpipc::Message {
    std::string name;
    std::string data;

    Message() {}

    Message(const std::string& _name, std::string&& _ustr)
        : name(_name)
        , data(std::move(_ustr))
    {
    }

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(data, "data").push(name, "name");
    }
};
```

## The client implementation

We will skip the instantiation of needed objects (Manager, mpipc::Service, Scheduler, Resolver) as they were presented in previous tutorials. Now we will continue with the configuration of mpipc::Service.

First initializing the protocol:
```C++
            auto   proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, proto);

            proto->registerType<Register>(con_register, 0, 10);
            proto->registerType<Message>(on_message, 1, 10);
```

where con_register is a lambda defined as follows:
```C++
            auto con_register = [](
                frame::mpipc::ConnectionContext& _rctx,
                std::shared_ptr<Register>&       _rsent_msg_ptr,
                std::shared_ptr<Register>&       _rrecv_msg_ptr,
                ErrorConditionT const&           _rerror) {
                SOLID_CHECK(!_rerror);

                if (_rrecv_msg_ptr and _rrecv_msg_ptr->name.empty()) {
                    auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
                        idbg("peerb --- enter active error: " << _rerror.message());
                        return frame::mpipc::MessagePointerT();
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
                frame::mpipc::ConnectionContext& _rctx,
                std::shared_ptr<Message>&        _rsent_msg_ptr,
                std::shared_ptr<Message>&        _rrecv_msg_ptr,
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

To register connection after it was established we need to configure mpipc::Service with a callback for connection start event:

```C++
    cfg.client.connection_start_fnc = on_connection_start;
```
where, _on_connection_start_ is a lambda:

```C++
            auto on_connection_start = [&p](frame::mpipc::ConnectionContext& _rctx) {
                idbg(_rctx.recipientId());

                auto            msgptr = std::make_shared<Register>(p.name);
                ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mpipc::MessageFlagsE::WaitResponse});
                SOLID_CHECK(not err, "failed send Register");
            };
```
which instantiates a Register request message and sends it to the server.

To activate a connection pool directed to the server, we need to call the following method after having successfully configured the mpipc::Service:

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
                    ipcservice.sendMessage(recipient.c_str(), make_shared<Message>(p.name, line.substr(offset + 1)), {frame::mpipc::MessageFlagsE::WaitResponse});
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
$ cd solid_frame_tutorials/mpipc_relay_echo
$ c++ -o mpipc_relay_echo_client mpipc_relay_echo_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
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
        frame::mpipc::relay::SingleNameEngine relay_engine(manager); //before relay service because it must outlive it
        frame::mpipc::ServiceT                ipcservice(manager);
        ErrorConditionT                       err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }
```

next follows a new block for configuring the mpipc::Service:

```C++
        {
            auto con_register = [&relay_engine](
                frame::mpipc::ConnectionContext& _rctx,
                std::shared_ptr<Register>&       _rsent_msg_ptr,
                std::shared_ptr<Register>&       _rrecv_msg_ptr,
                ErrorConditionT const&           _rerror) {
                SOLID_CHECK(!_rerror);
                if (_rrecv_msg_ptr) {
                    SOLID_CHECK(!_rsent_msg_ptr);
                    idbg("recv register request: " << _rrecv_msg_ptr->name);

                    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->name));

                    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                    SOLID_CHECK(!err, "Failed sending register response: " << err.message());

                } else {
                    SOLID_CHECK(!_rrecv_msg_ptr);
                    idbg("sent register response");
                }
            };

            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, relay_engine, proto);

            proto->registerType<Register>(con_register, 0, 10);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;
            cfg.relay_enabled = true;

            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            err = ipcservice.reconfigure(std::move(cfg));

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
    proto->registerType<Register>(con_register, 0, 10);
```

registers the con_register lambda to be called after a Register request message is received or response was sent back to the client.

All that the con_register lambda does, upon receiving a request, is to register the current, calling connection, onto relay_engine:

```C++
    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->name));
```

and send back the response

```C++
    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

    SOLID_CHECK(!err, "Failed sending register response: " << err.message());
```

going back to the configuration code, after configuring the listener, we enable the relay onto the server:

```C++
    cfg.relay_enabled = true;
```
and set Active as the start state for connections:

```C++
    cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
```

the last part of the configuration block handles the service _reconfigure_ call and, in case of success, it prints the port on which the server is listening on.

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
$ cd solid_frame_tutorials/mpipc_relay_echo
$ c++ -o mpipc_relay_echo_relay mpipc_relay_echo_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have both the client and the relay server applications, let us test them in a little scenario with two clients and a server.

**Console-1**:
```BASH
$ ./mpipc_relay_echo_server
```
**Console-2**:
```BASH
$ ./mpipc_relay_echo_client alpha
beta Some text sent from alpha to beta
beta Some other text sent from alpha to beta
```
**Console-3**:
```BASH
$ ./mpipc_relay_echo_client beta
alpha Some text sent from beta to alpha
alpha Some other text sent from beta to alpha
```

## Next

Well, this is the last tutorial for now.
If solid_frame_mpipc library or solid_frame has captured your attention, start using it and give feedback.

