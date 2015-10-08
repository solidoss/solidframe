# SolidFrame IPC module

A process-to-process communication (message exchange) engine via plain/secured TCP.

Enabled for two scenarios:
* client-server
* peer-to-peer

### Characteristics

* C++ only (no IDLs)
* Per destination connection pool.
* Buffer oriented message serialization engine. This means that the messages gets serialized (marshaled) one fixed sized buffer (e.g. 4KB) at a time which further enables sending messages that are bigger than the system memory (e.g. a message with an 100GB file).
* Message multiplexing. Multiple messages from the send queue are sent in parallel on the same connection. This means for example that one can send multiple small messages while also sending one (or more) huge message(s) like the one above.
* Support for buffer level compression. The library can compress (using a pluggable algorithm) a buffer before writing it on the socket.

## TODO

* Add support for canceling pending send Messages
* Prevent DOS made possible by sending lots of KeepAlive packets
	* ? set timer for non-active connections ?
	* ? count the number of received keep alive packets per interval ?
* Add support for "One Shot Delivery"
	* Add OneShot(/Send/Delivery)Flag
	* Normally, ipc service will keep on trying to send a message that:
		* Either is Idempotent and no response was received
		* Or it was not (partially) sent at all
	* OneShot Messages will fail after the first try
	* The Service will wait for the peer side to become available then send the messages.
* Add support for SSL
* Distributed chat example using frame/ipc
* SOCKS5 support.
