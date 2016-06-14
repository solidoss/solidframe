# SolidFrame

Cross-platform C++ framework for scalable, asynchronous, distributed client-server applications.

## License

Boost Software License - Version 1.0 - August 17th, 2003

## Prerequisites

* boost
* OpenSSL

## Supported platforms

C++11 enabled compilers on:

* Linux - (tested on latest Fedora) - gcc
* FreeBSD - (almost there) - llvm
* Darwin/OSX - (almost there) - llvm
* Windows - (partial) - latest VS

## Content

* System library wrapping up threads, synchronization, thread specific, file access, socket address, debug logging engine etc
* Asynchronous communication library for UDP and plain/secure (via OpenSSL library) TCP.
* Asynchronous multiplexed IPC (Inter Process Communication) library over secure/plain TCP ([IPC library](libraries/frame/ipc/README.md)). 
* Buffer oriented, asynchronous ready, binary serialization/marshalling engine - for binary protocols like the one used by IPC library.
* Shared object store with asynchronous read/write access.
* Shared file store with asynchronous read/write access.
* Sample applications

## TODO v2.0

* (PENDING) Finalizing the IPC library - what is planned for SFv2.0.
* Cross-platform support:
	* (DONE) Linux
	* (DONE) FreeBSD
	* (WAIT) Darwin/OSX (currently no support thread_local, will be supported by XCode 8)
* (DONE) Add "make install" support.
* Documentation.
* Preparing for SFv2.0.
* SolidFrame version 2.0.

## TODO v2.x

* frame/ipc with SOCKS5
* Improved OpenSSL support in frame/aio
* frame/ipc with OpenSSL

## See also
* [IPC library](libraries/frame/ipc/README.md)

