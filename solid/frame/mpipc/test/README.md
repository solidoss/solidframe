# SolidFrame MPIPC - Test Suite

### Basic protocol functionality

**Description**
* Serialize messages at one end and deserialize at the other

**Test**
* test_protocol_basic
	* directly use ipcmessagewriter and ipcmessagereader to serialize and deserialize messages
* test_protocol_synchronous
	* directly use ipcmessagewriter and ipcmessagereader to serialize and deserialize synchronous messages

### Basic IPC functionality

**Description**

* Client-Server
* Send messages and wait for response
* Connection pool

**Test**

* test_clientserver_basic
	* multiple messages are sent from client to server
	* the server respond back with the received message (echo)
	* messages are sent asynchronously and have different sizes
	* test paramenter: the number of connections for client connection pool.
	* test verify received message integrity
	* test verify that all responses are received during a certain amount of time
* test_clientserver_sendrequest
	* Mostly like above, only that there are two types of message: Request and Response
	* The client side uses ipc::Service::sendRequest() with callback for Response
* test_peer2peer_basic
	* multiple message are sent from both sides and the response is expected also on both sides
	* messages are sent asynchronously and have different sizes
	* test paramenter: the number of connections per pool
	* test verify received message integrity
	* test verify that all responses are received during a certain amount of time

### Prevent DOS made possible by sending lots of KeepAlive packets

**Description**

* set timer for non-active connections
* count the number of received keep alive packets per interval

**Test**

* test_keepalive_fail
	* client sends one message to server
	* test parameter: scenario 0: test that server closes connection with error_too_many_keepalive_packets_received
	* test parameter: scenario 1: test that server closes connection with error_inactivity_timeout
	* scenario 0: client is configured to send multiple keep-alive packets
	* scenario 0: server is configured so that it should close connection because of multime keep-alive packets
	* scenario 1: client is configured to send keep-alive packet very rarely
	* scenario 1: server is configured so that is should close connection because of inactivity
* test_keepalive_success
	* scenario 0: verify that connection is not closed during inactivity period
	* scenario 0: a message is send from client after an inactivity period and a response is expected from server


### Support for One-Shot-Delivery and Message Canceling

**Description**

* Normal behavior: ipc::service will continuously try to send a message until:
	* Normal message: fully or partially sent
	* Idempotent messages:
		* when fully sent
		* or, if expecting response, until receiving the response
* Will wait for the peer side to become available then send the messages
* For Messages with OneShotSendFlag, ipc::service will try to call complete asap - will imediately fail if no connection to server
* Canceling a message means that:
	* no "complete" callback will be called on the message
	* if the message is in a cancelable state (is not currently being sent or is not already sent and waiting for a response) the message is dropped from the send queue.


#### Unnamed connection pools for server-side messages.

Queues

* Messages are allways pushed on the pool's message queue - 2 locks one for Service and one for connection pool.
* Connection's are either notified with NewPoolMessageEvent or NewConnectionMessageEvent containing the MessageId
* Writer will only have a writing queue and a response waiting queue.
* Connection will have a queue of MessageIds, for NewConnectionMessageEvents that could not be directly delivered to writer.

Locking

* 2 locks on pushing message
* 1 lock for sending the event to connection
* 1 lock for receiving the event on connection
* ... TODO ...

**Test**

* test_clientserver_delayed
	* start client and send message
	* wait a while
	* start the server and expect a response on the client side
	* try to cancel the sent message - expect fail
* test_clientserver_noserver
	* start client and send message
	* wait for a while - expect message did not complete somehow
	* cancel the message - expect message to complete with error_canceled
* test_clientserver_oneshot
	* start client and send one shot message
	* expect message canceled in certain, short amount of time
* test_clientserver_idempotent
	* start client and send asynchronously:
		* one long idempotent message
		* one long message
		* one one shot, short message
	* when server receives the one shot message stop the server, pause, then restart it
	* expect the idempotent message be resent.
	* expect the second long message not be resent

### Support for multi-protocol

**Description**

We want to be able to easily specify on Client side a subset of Messages supported on Server.

message_type_id = protocol_index + type_index;

**Test**

* test_multiprotocol
	* start a server ipc configure it with 3 protocols (3 groups of messages)
	* start 3 different clients configured with different protocols
	* clients exchange some messages with the server

### Support for packet compression

**Test**

* test_clientserver_basic
	* add support for compression using Google Snappy (http://google.github.io/snappy/)

### Support for SSL

We want to be able to use Secure TCP connections as transport layer.

### Support for SOCKS5 and alike

Connections can be started in Raw mode, which means that sending and receiving data on the corresponding socket
is controlled from the outside of connection through specific events.

Implementing SOCKS5 support also needs:
	* Resolve function should return the IPs of the SOCKS5 servers
	* Connections should be able to receive and send raw data before startTLS and before Activate.

Different connection scenarios:
* secureConnection -> authentication handshake.. -> activateConnection
* ...plain initialization handshake... -> (startTLS command?!) secureConnection -> authentication handshake... -> activateConnection
* ...raw handshake... -> secureConnection -> authentication handshake.. -> activateConnection
* ...raw handshake... -> ...plain initialization handshake... -> (startTLS command?!) secureConnection -> authentication handshake... -> activateConnection

Connection states/flags:
	* RawState:			sending and receiving raw data
	* InnactiveState:	send only direct connection messages, receive any message
	* SecureFlag:		when set, connection start ssl handshake then on success same as InnactiveState
	* ActiveState:		send any kind of message

SecureFlag can be set while on InnactiveState or ActiveState but not on RawState.
SecureFlag also cannot be set for connections with no SSL support.

A call to notifyConnectionSecure for the above invalid situations, will close connection with specific error.

**Test**

* test_raw_basic
	* Both server and client connections start in RawState
	* The client raw sends a big string using serialization or other mechanism
	* The server raw receives the big string, sends it back then activates the connection
	* When the client receives back the big string, activates the connection and send messages in the pool
	* Expect: the big string is correctly transferred and the messages in the queue are sent correctly.
* test_raw_proxy
	* Implement a proxy/relay server.
		* Expect connection with RelayHeader{string very_long_data; string destination; string port;}.
		* Check header.very_long_data.
		* Resolve destination and connect to it.
		* Respond '0' if OK, '1' if fail.
	* Implement a client as for clientserver_basic, but start connections in RawState and do not resove the destination but send within in RelayHeader.
	* Implement a server as for clientserver_basic.
