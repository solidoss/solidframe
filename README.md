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

## See also
* [IPC library](frame/ipc/README.md)

