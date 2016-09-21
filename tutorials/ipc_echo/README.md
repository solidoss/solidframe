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
