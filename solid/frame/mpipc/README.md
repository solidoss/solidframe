# solid_frame_mpipc

**Message Passing InterProcess Communication Engine TCP**

## Features

* C++ only (no IDLs) with easy to use **asynchronous API**.
* A single class (solid::frame::mpipc::Service) for both client and server. An instance of solid::frame::mpipc::Service can act as a client as a server or as both.
* Buffer oriented message serialization engine: messages are serialized (marshaled) one fixed size buffer at a time, further enabling:
	* **No limit for message size** - one can send a 100GB file as a single message.
	* **Message multiplexing** - messages from the send queue are sent in parallel on the same connection. This means for example that multiple small messages can be send while also sending one (or more) huge message(s).
* For client side, use **connection pool per recipient**.
	* By default the connection pool is limited to a single connection.
	* For higher throughput one can increase this limit in mpipc::Service's configuration.
* Rescale up after a network failure.
* Messages can be any of the following types:
	* __basic__: normal behavior
		* In case of network failures, the library will keep on trying to send the message until the message Started to be sent.
		* If, while sending the message, there is a network failure the library will complete it immediately and not try to resend it.
	* __synchronous__: all synchronous messages are sent one after another.
	* __one_shot__: only tries once to send the message.
	* __idempotent__: will try re-sending the message until either successfully sent (i.e. completely left the sending side) or, in case the message awaits a response, until a response was received.

**solid_frame_mpipc** is a peer-to-peer message passing communication library which provides a pure C++ way of implementing communication between two processes. It uses:
 * asynchronous (Secure/Plain) TCP connection pools through solid_frame_aio library.
 * a serialization protocol based on solid_serialization library.

Thus, solid_frame_mpipc differs from other implementations by:
 * not needing a message pre-processor for marshaling (as does: Google protobuf) - you specify how a message gets marshaled using simple C++ code (something similar to boost-serialization).
 * not needing a pre-processor for creating client server stubs (Apache thrift) - you just instantiate a frame::mpipc::ServiceT allong with its dependencies and configure it.

The downside is that solid_frame_mpipc will always be a C++ only library while the above alternatives (protobuf & thrift) can be used from multiple languages.

On the other hand you should be able to call native C++ code from other languages.

### Planned
* Support for buffer/packet level compression.
	* The library will compress/decompress using a pluggable algorithm.
	* In future versions of the library the compression will be adaptable.
* **Finalize support for Secure Communication** after solid_frame_aio_openssl library will be completed.

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


## TODO v2.x

* test_unresolved_recipient
* test_raw_proxy
* add support for compression - test and improve
* add support for OpenSSL - needs extending OpenSSL support in solid_frame_aio_openssl
* add support in mpipc::configuration for SOCKS5

## NOTES
1. closeConnection can be used for closing a connection after a certain message was sent.

## See also
* [MPIPC Test Suite](test/README.md)
