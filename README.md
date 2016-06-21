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
* FreeBSD - (tested on FreeBSD/PcBSD 10.3) - llvm
* Darwin/OSX - (waiting for XCode 8 with support for thread_local) - llvm
* Windows - (partial) - latest VS

## Libraries

* solid_system:
	* Wrappers for socket/file devices, socket address, directory access
	* Debug log engine
* solid_utility:
	* any - similar to boost::any
	* event - Event class containing an ID an solid::Any object and a Category (similar to std::error_category)
	* innerlist - bidirectional list mapped over a vector/deque
	* stack - alternative to std::stack
	* queue - alternative to std:queue
	* workpool - generic thread pool
* solid_serialization: binary serialization/marshalling
* solid_frame:
	* object: reactive object
	* manager: store of services and notifies objects
	* service: store and notifies objects
	* reactor: active store of objects - allows objects to asynchronously react on events
	* scheduler: a thread pool of reactors
	* timer
	* shared store: generic store of shared objects that need either multiple read or single write access
* solid_frame_aio: asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
	* object: reactive object with support for Asynchronous IO
	* reactor: reactor with support for Asynchronous IO
	* listener: asynchronous TCP listener/server socket
	* stream: asynchronous TCP socket
	* datagram: asynchronous UDP socket
	* timer
* solid_frame_aio_openssl
	* SSL support via OpenSSL
* solid_frame_file
	* file store: a shared store for files
* solid_frame_ipc: asynchronous Secure/Plain TCP inter-process communication engine ([IPC library](libraries/frame/ipc/README.md))
	* ipcservice

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

