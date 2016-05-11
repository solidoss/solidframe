# SolidFrame IPC module

A process-to-process communication (message exchange) engine via plain/secured TCP.

## Algorithm for creating pool connections

## Characteristics

* C++ only (no IDLs).
* Connection pool per destination.
* Asynchronous.
* Buffer oriented message serialization engine: the messages get serialized (marshaled) one fixed sized buffer (e.g. 4KB) at a time. This further enables:
	* Sending messages that are bigger than the system memory (e.g. a message with a 100GB file).
	* Message multiplexing. Multiple messages from the send queue are sent in parallel on the same connection. This means for example that one can send multiple small messages while also sending one (or more) huge message(s) like the one above.
* Support for buffer level compression. The library can compress (using a pluggable algorithm) a buffer before writing it on the socket. The intent is to make the commpession adaptable.
* Able to limit the number of pending connections - if a name resove returns 100 IP addresses, it does not create as many connections.
* Scale up the number of connections based on the number of messages on queue and up to a configurable limit
* Rescale up after a network failure
* Messages can be any of:
	* synchronous: all synchronous messages are sent one after another.
	* one_shot: only tries once to send the message
	* idempotent: will try resending the message until either successfully sent or, in case the message awaits a response, until a response was received. 
* By default, the IPC engine will try resend a message if not yet started to be sent. If a message started to be sent and the connection close will be rescheduled for sending ONLY if it is idempotent otherwise will be completed with error.



	
## TODO v2.0:

* test_raw_basic
* test_raw_proxy
* test_multiprotocol
* test_clientserver_idempotent
* test_pool_force_close
* test_pool_delay_close
* (DONE) test_clientserver_delayed
* (DONE) test_clientserver_noserver
* (DONE) test_clientserver_oneshot
* (DONE) set all ErrorConditions across frame/aio and frame/ipc to valid errors (all the TODOs)
* (DONE) investigate how to support multiple versions of the serialization library.
* (DONE) add support multiple versions of the serialization library

## TODO v2.x

* add support in ipc::configuration for SOCKS5
* add support for OpenSSL (needs extending OpenSSL support in frame/aio)


## See also
* [IPC Test Suite](test/README.md)
