# solid_frame_ipc

A peer-to-peer communication (message exchange) engine via plain/secured TCP.

## Features

* C++ only (no IDLs).
* Per recipient connection pool.
	* Scale up the number of connections based on the number of messages on send queue and up to a configurable limit.
* Asynchronous.
	* The interface is asynchronous.
	* Uses solid::frame::aio, asynchronous communication library.
* Buffer oriented message serialization engine: the messages is serialized (marshaled) one fixed size buffer at a time. This further enables:
	* Sending messages that are bigger than the system memory (e.g. a message with a 100GB file).
	* Message multiplexing. Messages from the send queue are sent in parallel on the same connection. This means for example that multiple small messages can be send while also sending one (or more) huge message(s).
* Configurable limit for per-recipient pending number of connections.
* Rescale up after a network failure.
* Messages can be any of:
	* __basic__: normal behavior
		* In case of network failures, the library will keep on trying to send the message until the message Started to be sent.
		* If, while sending the message, there is a network failure the library will complete it immediately and not try to resend it.
	* __synchronous__: all synchronous messages are sent one after another.
	* __one_shot__: only tries once to send the message.
	* __idempotent__: will try resending the message until either successfully sent or, in case the message awaits a response, until a response was received. 

_solid_frame_ipc_ is a peer-to-peer communication library which provides a pure C++ way of implementing communication between two processes. It offers:
 * asynchronous (Secure/Plain) TCP connection pools, using solid_frame_aio library.
 * a serialization protocol based on solid_serialization library.

Thus, solid_frame_ipc differs from other implementations by:
 * not needing a message preprocessor for marshaling (as does: protobuf) - you specify how a message gets marshalled programatically using simple C++ code (something similar to boost-serialization).
 * not needing a preprocessor for creating client server stubs (apache thrift) - you just instantiate a frame::ipc::ServiceT allong with its dependecies and configure it.

The downside is that solid_frame_ipc will always be a C++ only library while the above alternatives (protobuf & thrift) can be used from multiple languages.

On the other side you should be able to call native C++ code from other languages.

### Planned
* Support for buffer/packet level compression.
	* The library will compress/decompress using a pluggable algorithm.
	* In future versions of the library the compression will be adaptable.
* Support SSL/TLS for ecrypted transport (using OpenSSL)

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
* (DONE) set all ErrorConditions across frame/aio and frame/ipc to valid errors (all the TODOs)
* (DONE) investigate how to support multiple versions of the serialization library.
* (DONE) add support multiple versions of the serialization library

## TODO v2.x

* test_unresolved_recipient
* test_raw_proxy
* add support for compression - test and improve
* add support for OpenSSL - needs extending OpenSSL support in frame/aio
* add support in ipc::configuration for SOCKS5

## NOTES
1. closeConnection can be used for closing a connection after a certain message was sent.

## See also
* [IPC Test Suite](test/README.md)
