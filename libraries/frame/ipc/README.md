# SolidFrame IPC module

A process-to-process communication (message exchange) engine via plain/secured TCP.

Enabled for two scenarios:
* client-server
* peer-to-peer

## Characteristics

* C++ only (no IDLs)
* Per destination connection pool.
* Buffer oriented message serialization engine. This means that the messages gets serialized (marshaled) one fixed sized buffer (e.g. 4KB) at a time which further enables sending messages that are bigger than the system memory (e.g. a message with an 100GB file).
* Message multiplexing. Multiple messages from the send queue are sent in parallel on the same connection. This means for example that one can send multiple small messages while also sending one (or more) huge message(s) like the one above.
* Support for buffer level compression. The library can compress (using a pluggable algorithm) a buffer before writing it on the socket.

## Implemented Functionality

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
* Peer2Peer
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


## Pending Functionality

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

Because the code got very complicated because of the support for message canceling, a refactoring is needed.

#### Current state

For server only
* Messages enter directly in Connection's receiving vector (there are 2 for locking eficiency) - one lock on Connection's mutex
* On the Connection's thread, {lock Connection's mutex, switch the receiving vector} move message from receiving vector to writer
* If writer's writing queue is full, put message in waiting queue.
* The writer also needs a synchronized queue of cached positions on writer

so, we have for queues:
* 3 queues for messages
* 1 unsynchronized queue for writer cached positions
* 1 synchronized queue for writer cached positions

and for locks:
* 2 at push - for for Service and one for Connection
* 1 on Connection's thread on switching receiving vectors
* 1 for synchronizing writer caches


#### Wanted state

Now we want to use unnamed connection pools for server-side messages.

Queues

* Messages are allways pushed on the pool's message queue - 2 locks one for Service and one for connection pool.
* Connection's are either notified with NewMessageEvent or SpecificNewMessageEvent containing the MessageId
* Writer will only have one, writing queue.
* Connection will have a queue of MessageIds, for SpecificNewMessageEvents that could not be directly delivered to writer.

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
* test_clientserver_cancel_connection_switch(TODO)
	* test that message cancel connection switch works

### Support for multi-protocol

**Description**

* Extend ipc::Service to:
	* Aupport a vector of TypeIdMapT
	* Add setConnectionProtocol(ObjectUid const&, size_t protocol_id)
	* Default connection protocol is 0 (zero)
	* All connection init messages must be known by protocol 0
	* On init message, the server ipc::Service must change the connection protocol
		on message_received callback by calling setConnectionProtocol

**Test**
* test_clientserver_multiproto_basic
	* start a server ipc configure it with 3 protocols (0 for init, 1 and 2 for different clients)
	* start 2 different clients configured with different protocols
	* proceed on both protcols as in test_clientserver_basic.

### Support for packet compression

**Test**

* test_clientserver_basic
	* add support for compression using Google Snappy (http://google.github.io/snappy/)

### Support for SSL

### Support for SOCKS5
