# solid::frame::mpipc relay echo tutorial

Exemplifies the use of solid_frame_mpipc for relaying messages between connections.

__Source files__
 * Register message definition: [mpipc_relay_echo_register.hpp](mpipc_relay_echo_register.hpp)
 * The relay server: [mpipc_relay_echo_relay.cpp](mpipc_relay_echo_relay.cpp)
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
 * the one between clients and the releay server, used for registering connections onto relay server: Register message
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



### Compile

```bash
$ cd solid_frame_tutorials/mpipc_relay_echo
$ c++ -o mpipc_relay_echo_client mpipc_relay_echo_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The relay server implementation



### Compile

```bash
$ cd solid_frame_tutorials/mpipc_relay_echo
$ c++ -o mpipc_relay_echo_relay mpipc_relay_echo_relay.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have the client and the relay server applications, let us test them in a little scenario with two clients and a server.

**Console-1**:
```BASH
$ ./mpipc_relay_echo_relay
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

