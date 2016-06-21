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

* __solid_system__:
	* Wrappers for socket/file devices, socket address, directory access
	* Debug log engine
* __solid_utility__:
	* _any_ - similar to boost::any
	* _event_ - Event class containing an ID an solid::Any object and a Category (similar to std::error_category)
	* _innerlist_ - bidirectional list mapped over a vector/deque
	* _stack_ - alternative to std::stack
	* _queue_ - alternative to std:queue
	* _workpool_ - generic thread pool
* __solid_serialization__: binary serialization/marshalling
* __solid_frame__:
	* _object_ - reactive object
	* _manager_ - store of services and notifies objects
	* _service_ - store and notifies objects
	* _reactor_ - active store of objects - allows objects to asynchronously react on events
	* _scheduler_ - a thread pool of reactors
	* _timer_
	* _shared store_ - generic store of shared objects that need either multiple read or single write access
* __solid_frame_aio__: asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
	* _object_ - reactive object with support for Asynchronous IO
	* _reactor_ - reactor with support for Asynchronous IO
	* _listener_ - asynchronous TCP listener/server socket
	* _stream_ - asynchronous TCP socket
	* _datagram_ - asynchronous UDP socket
	* _timer_
* __solid_frame_aio_openssl__: SSL support via OpenSSL
* __solid_frame_file__
	* _file store_ - a shared store for files
* __solid_frame_ipc__: asynchronous Secure/Plain TCP inter-process communication engine ([IPC library](libraries/frame/ipc/README.md))
	* _ipcservice_

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

