# SolidFrame

Cross-platform C++ framework for scalable, asynchronous, distributed client-server applications.

## License

Boost Software License - Version 1.0 - August 17th, 2003

## Prerequisites

* boost
* OpenSSL

## Supported platforms

C++11 enabled compilers on:

* Linux - (tested on Fedora 20 to 22) - gcc
* FreeBSD - (almost there) - llvm
* Darwin/OSX - (almost there) - llvm
* Windows - (partial) - latest VS

## Content

* System library wrapping up threads, synchronization, thread specific, file access, socket address, debug logging engine etc
* Asynchronous communication library for UDP and plain/secure (via OpenSSL library) TCP.
* Asynchronous multiplexed IPC (Inter Process Communication) library over secure/plain TCP. See more information below.
* Buffer oriented, asynchronous ready, binary serialization/marshaling engine - for binary protocols like the one used by IPC library.
* Shared object store with asynchronous read/write access.
* Shared file store with asynchronous read/write access.
* Implementation of Fast-Multi-Paxos consensus algorithm (!! Needs to be refactored to use the latest IPC library !!).
* Sample applications
 
## frame/ipc library

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

### TO DO:

* Add SSL support
* Distributed chat example using frame/ipc
* SOCKS5 support.
