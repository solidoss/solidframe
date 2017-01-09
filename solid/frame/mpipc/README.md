# solid_frame_mpipc: Message Passing Inter-Process Communication Engine

## Features

 * C++ only (no IDLs) with easy to use **asynchronous API**.
 * A single class (solid::frame::mpipc::Service) for both client and server. An instance of solid::frame::mpipc::Service can act as a client, as a server, or as both.
 * Pluggable - i.e. header only - secure communication support via solid_frame_aio_openssl (wrapper over OpenSSL1.1.0/BoringSSL).
 * Pluggable - i.e. header only - communication compression support via [Snappy](https://google.github.io/snappy/)
 * Pluggable - i.e. header only - protocol based on solid_serialization - a buffer oriented message serialization engine. Thus, messages are serialized (marshaled) one fixed size buffer at a time, further enabling:
    * **No limit for message size** - one can send a 100GB file as a single message.
    * **Message multiplexing** - messages from the send queue are sent in parallel on the same connection. This means for example that multiple small messages can be sent while also sending one (or more) bigger message(s).
 * For client side, use **connection pool per recipient**.
    * By default the connection pool is limited to a single connection.
    * For higher throughput one can increase this limit in mpipc::Service's configuration.
 * Rescale up after a network failure.
 * Messages can be any of the following types:
    * __basic__: normal behavior
        * In case of network failures, the library will keep on trying to send the message until the message has Started to be sent.
        * If, while sending the message, there is a network failure the library will complete it immediately and not try to resend it.
    * __synchronous__: all synchronous messages are sent one after another.
    * __one_shot__: only tries once to send the message.
    * __idempotent__: will try re-sending the message until either successfully sent (i.e. completely left the sending side) or, in case the message awaits a response, until the response was received.

__NOTE__: The header only plugins ensure that solid_frame_mpipc itself does not depend on the libraries the plugins depend on.

**solid_frame_mpipc** is a peer-to-peer message passing communication library which provides a pure C++ way of implementing communication between two processes. It uses:
 * asynchronous secure/plain TCP connection pools through solid_frame_aio library.
 * a serialization protocol based on solid_serialization library.

Thus, solid_frame_mpipc differs from other implementations by:
 * not needing a message pre-processor for marshaling (as does: Google protobuf) - you specify how a message gets marshaled using simple C++ code (something similar to boost-serialization).
 * not needing a pre-processor for creating client server stubs (Apache thrift) - you just instantiate a frame::mpipc::ServiceT allong with its dependencies and configure it.

The downside is that solid_frame_mpipc will always be a C++ only library while the above alternatives (protobuf & thrift) can be used from multiple languages.

On the other hand you should be able to call native C/C++ code from other languages.
Here are two examples of Android applications using solid_frame_mpipc to communicate with a central server:
 * [Bubbles](https://github.com/vipalade/bubbles) - C++ Linux server + Qt Linux Client + Android client application all using solid_frame_mpipc with secure communication
 * [EchoClient](https://github.com/vipalade/study_android/tree/master/EchoClient) - C++ Linux server + Android client application all using solid_frame_mpipc with secure communication

Both examples implement the communication and application logic in a C++ library and use a JNI (Java Native Interface) _facade_ for interacting with Android Java user interface code.

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
