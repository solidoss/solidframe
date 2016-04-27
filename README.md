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

## TODO

* Finalizing the IPC library - what is planned for SFv2.0.
* Cross-platform support: FreeBSD, Darwin/OSX, Windows (partial).
* Add "make install" support.
* Preparing for SFv2.0.
* SolidFrame version 2.0.
* frame/ipc with SOCKS5 (SFv2.1)
* Improved OpenSSL support in frame/aio (SFv2.2)
* frame/ipc with OpenSSL (SFv2.2)


## See also
* [IPC library](libraries/frame/ipc/README.md)

