# solid_frame_mpipc: Message Passing - Inter Process Communication Engine

## Features

 * C++ only (no IDLs/proto files) with easy to use **asynchronous API**.
 * Supported modes: client, server and relay.
 * A single class (solid::frame::mpipc::Service) for all modes. An instance of solid::frame::mpipc::Service can act as any combinations of client, server or relay engine.
 * Pluggable - i.e. header only - secure communication support via solid_frame_aio_openssl (wrapper over OpenSSL1.1.0/BoringSSL).
 * Pluggable - i.e. header only - communication compression support via [Snappy](https://google.github.io/snappy/)
 * Pluggable - i.e. header only - protocol based on solid_serialization - a buffer oriented message serialization engine. Thus, messages are serialized (marshaled) one fixed size buffer at a time, further enabling:
    * **No limit for message size** - one can send a 100GB file as a single message.
    * **Message multiplexing** - messages from the send queue are sent in parallel on the same connection. This means for example that multiple small messages can be sent while also sending one (or more) bigger message(s).
 * For client side, use **connection pool per recipient**.
    * By default the connection pool is limited to a single connection.
    * For higher throughput one can increase this limit in mpipc::Service's configuration.
 * Messages can be of any of the following types:
    * __basic__: normal behavior, i.e.:
        * In case of network failures, the library will keep on trying to send the message until the message has Started to be sent.
        * If, while sending the message, there is a network failure the library will complete it immediately and not try to resend it.
    * __synchronous__: (flag) all synchronous messages are sent one after another.
    * __one_shot__: (flag) only tries once to send the message.
    * __idempotent__: (flag) will try re-sending the message until either successfully sent (i.e. completely left the sending side) or, in case the message awaits a response, until the response was received.

__NOTE__: The header only plugins ensure that solid_frame_mpipc library itself does not depend on the libraries the plugins depend on.

**solid_frame_mpipc** is a peer-to-peer message passing communication library which provides a pure C++ way of implementing communication between two processes. It uses:
 * asynchronous secure/plain TCP connection pools through solid_frame_aio library.
 * a serialization protocol based on solid_serialization library.

**solid_frame_mpipc** differs from other implementations by:
 * not needing a message pre-processor for marshaling (as do: Google gRPC, Apache Thrift) - you specify how a message gets marshaled using simple C++ code (something similar to boost-serialization, see below).
 * not needing a pre-processor for creating client server stubs (as do: Google gRPC, Apache Thrift) - you just instantiate a frame::mpipc::ServiceT allong with its dependencies and configure it.

The downside is that solid_frame_mpipc will always be a C++ only library while the above alternatives (gRPC & Thrift) can be used with multiple languages.

On the other hand you should be able to call native C/C++ code from other languages.

Here are two examples of Android applications using solid_frame_mpipc to communicate with a central server:
 * [Bubbles](https://github.com/vipalade/bubbles) - C++ Linux server + Qt Linux Client + Android client application all using solid_frame_mpipc with secure communication
 * [EchoClient](https://github.com/vipalade/study_android/tree/master/EchoClient) - C++ Linux server + Android client application all using solid_frame_mpipc with secure communication

Both examples implement the communication and application logic in a C++ library and use a JNI (Java Native Interface) _facade_ for interacting with Android Java user interface code.

### Serialization Engine

The default pluggable serialization engine is based on solid_serialization library, which resembles somehow boost_serialization/cereal libraries.

The main difference, comes from the fact that solid_serialization is asynchronous enabled while the others are synchronous.

Synchronous serialization means that a message can be started to be deserialized (e.g. reconstructed on peer process) only after the entire serialization data is present.

With asynchronous serialization engines the deserialization starts with the first byte received and continues with every data part received, this way you end-up using less memory (useful for big messages) and you get more flexibility in passing streams (e.g. file streams) from one side to the other (follow this [tutorial](../../../tutorials/mpipc_file) for more details).

Here are some advantages I have observed for the asynchronous model while having used both models in network communication libraries:
 
 * more "natural" integration with an asynchronous communication library (like solid_frame_aio or boost_asio);
 * more "natural" support for transmitting streams (e.g. files)
 * easier to implement message multiplexing.
 * a lot more control over serialization / deserialization with support for imposing item limits - e.g. for a certain message, we can impose a limit for all strings to be less than 1K in size - the serializer / deserializer will immediately error on the first string exceeding the limit (for now, solid_serialization library supports three kinds of limits: for string size, for container size and for stream size).


## <a id="relay_engine"></a>Relay Engine

The relay engine enables transparent message forwarding from one connection to another.

The MPIPC library is mostly designed as a message passing bus for cloud infrastructures. The relay engine, comes from the need to transparently multiplex messages coming from multiple connections on (few) other (e.g. service) connections.

As described above, one advantage the solid_frame_mpipc library has over other message passing implementations is the fact that there is no actual limit on the size of the messages  it transfers. The same rule applies also to relayed messages thus, the relay engine is able to forward messages of any size from one connection to another.
Though, not yet tested, the engine is designed to work in a mesh of nodes which can relay data from one connection to another not necessarily both on the same node.

The aim of the relay support in the MPIPC library, is to enable cloud infrastructures where (hundreds of) millions of connections can exchange messages with each other.

Follow this [tutorial](../../../tutorials/mpipc_relay_echo) for details.

## Backlog
* solid_frame_mpipc: test_unresolved_recipient
* solid_frame_mpipc: test_raw_proxy
* solid_frame_mpipc: SOCKS5
* solid_frame_mpipc: test with thousands of connections

## TODO v2.1
* (DONE) solid_frame_mpipc: Pluggable (header only) support for SSL
* (DONE) solid_frame_mpipc: Pluggable (header only) basic compression support using [Snappy](https://google.github.io/snappy/)


## TODO v2.0:
* (DONE) connection should fail if recipient name is not resolved - should not retry send messages
* (DONE) test_raw_basic
* (DONE) test_multiprotocol
* (DONE) test_connection_close
* (DONE) add Service::closeConnection(RecipientId) (see note 1)
* (DONE) test_pool_force_close
* (DONE) test_pool_delay_close
* (DONE) test_clientserver_idempotent
* (DONE) test_clientserver_delayed
* (DONE) test_clientserver_noserver
* (DONE) test_clientserver_oneshot
* (DONE) set all ErrorConditions across frame/aio and frame/mpipc to valid errors (all the TODOs)
* (DONE) investigate how to support multiple versions of the serialization library.
* (DONE) add support multiple versions of the serialization library
* (DONE) allow for a response to access the request message from its serialization method.

### NOTES
1. closeConnection can be used for closing a connection after a certain message was sent.

## See also
* [MPIPC Test Suite](test/README.md)
