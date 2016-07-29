# SolidFrame

Cross-platform C++ framework for scalable, asynchronous, distributed client-server applications.

## License

Boost Software License - Version 1.0 - August 17th, 2003

## Resources
 * [API Reference](solid/API.md)
 * [Tutorials](tutorials/README.md)
 * [Wiki](https://github.com/vipalade/solidframe/wiki)
 * [Release Notes](RELEASES.md)
 * [IPC library](solid/frame/ipc/README.md)

## Prerequisites
* C++11 enabled compiler
* [CMake](https://cmake.org/) 
* [boost](http://www.boost.org/)
* [OpenSSL](https://www.openssl.org/)

## Supported platforms

* Linux - (tested on latest Fedora i686/x86_64 and Raspian on Raspberry Pi 2 armv7l) - gcc
* FreeBSD - (tested on FreeBSD/PcBSD 10.3) - llvm
* Darwin/OSX - (waiting for XCode 8 with support for thread_local) - llvm
* Windows - (partial) - latest VisualStudio

## Libraries

* __solid_system__:
	* Wrappers for socket/file devices, socket address, directory access
	* Debug log engine
* __solid_utility__:
	* _Any_ - similar to boost::any
	* _Event_ - Event class containing an ID a solid::Any object and a Category (similar to std::error_category)
	* _InnerList_ - bidirectional list mapped over a vector/deque
	* _Stack_ - alternative to std::stack
	* _Queue_ - alternative to std:queue
	* _WorkPool_ - generic thread pool
* __solid_serialization__: binary serialization/marshalling
	* _binary::Serializer_
	* _binary::Deserializer_
	* _TypeIdMap_
* __solid_frame__:
	* _Object_ - reactive object
	* _Manager_ - store services and notifies objects within services
	* _Service_ - store and notifies objects
	* _Reactor_ - active store of objects - allows objects to asynchronously react on events
	* _Scheduler_ - a thread pool of reactors
	* _Timer_ - allows objects to schedule time based events
	* _shared::Store_ - generic store of shared objects that need either multiple read or single write access
* __solid_frame_aio__: asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
	* _Object_ - reactive object with support for Asynchronous IO
	* _Reactor_ - reactor with support for Asynchronous IO
	* _Listener_ - asynchronous TCP listener/server socket
	* _Stream_ - asynchronous TCP socket
	* _Datagram_ - asynchronous UDP socket
	* _Timer_ - allows objects to schedule time based events
* __solid_frame_aio_openssl__: SSL support via OpenSSL
* __solid_frame_file__
	* _file::Store_ - a shared store for files
* __solid_frame_ipc__: asynchronous Secure/Plain TCP inter-process communication engine ([IPC library](solid/frame/ipc/README.md))
	* _ipc::Service_ - asynchronously sends and receives messages to/from multiple peers.

## Installation

Following are the steps for:
* fetching the _SolidFrame_ code
* building the prerequisites folder
* building and installing _SolidFrame_
* using _SolidFrame_ in your projects

Please note that [boost framework](http://www.boost.org) is only used for building _SolidFrame_.
Normally, _SolidFrame_ libraries would not depend on boost.

### Linux/macOS/FreeBSD

System prerequisites:
 * C++11 enabled compiler: gcc-c++ on Linux and clang on FreeBSD and macOS (minimum: XCode 8/Clang 8).
 * [CMake](https://cmake.org/)

Bash commands for installing _SolidFrame_:

```bash
$ mkdir ~/work
$ cd ~/work
$ git clone git@github.com:vipalade/solidframe.git
$ mkdir extern
$ cd extern
$ ../solidframe/prerequisites/prepare_extern.sh
# ... wait until the prerequisites are built
$ cd ../solidframe
$ ./configure -e ~/work/extern --prefix ~/work/extern
$ cd build/release
$ make install
# ... when finished, the header files will be located in ~/work/extern/include/solid
# and the libraries at ~/work/extern/lib/libsolid_*.a
```
### Windows
Windows is not yet supported.

## Overview

_SolidFrame_ is an experimental framework to be used for implementing cross-platform C++ network applications.

The consisting libraries only depend on C++ STL with two exceptions:
 * __solid_frame_aio_openssl__: which obviously depends on [OpenSSL](https://www.openssl.org/)
 * __solid_frame_ipc__: which, by using solid_frame_aio_openssl implicitly depends on OpenSSL too.

In order to keep the framework as dependency-free as possible some components that can be found in other libraries/frameworks were re-implemented here too, most of them being locate in either __solid_utility__ or __solid_system__ libraries.

In the next paragraphs I will briefly present every library.

### solid_system

The library consists of wrappers around system calls for:
 * _device.hpp_, _seekabledevice.hpp_, _filedevice.hpp_, _socketdevice.hpp_: devices (aka descriptors/handles) for: files, sockets etc.
 * _socketaddress.hpp_: socket addresses, synchronous socket address resolver
 * _nanotime.hpp_: high precision clock
 * _directory.hpp_: basic file-system directory operations
 * _debug.hpp_: debug log engine

The most notable component is the debug log engine which allows sending log records to different locations:
  * file
  * stderr
  * socket endpoint

The library defines some macros for easily specify log lines. The macros are only active (do something) when SolidFrame is compiled with SOLID_HAS_DEBUG (e.g. maintain and debug builds).
The library has support for registering modules and for specifying enabled log levels. Here is an example:

```C++
	Debug::the().levelMask("view");
	Debug::the().moduleMask("frame_ipc:iew any:ew");
	Debug::the().initStdErr();
	//...
	edbg("starting aio server scheduler: "<<err.message());
	//...
	idbgx(Debug::ipc, this<<" enqueue message "<<_rpool_msg_id<<" to connection "<<this<<" retval = "<<success);
```

In the above code we:
 * Set the global levelMask to "view" (V = Verbose, I = Info, E = Error, W = Warning).
 * Only enable logging for "frame_ipc" and "any" (the generic module used with [view]dbg()). For "frame_ipc" restrict the level mask to {Info, Error, Warning} and for "any" restrict it to only {Error and Warning}.
 * Configure the debug log engine to send log records to stderr.
 * Send a log _error_ line for "any" module.
 * Send a log _info_ line for "frame_ipc" module.
 
### solid_utility

The library consists of tools needed by upper level libraries:
 * _algorithm.hpp_: Some inline algorithms
 * _any.hpp_: A variation on boost::any / experimental std::any with storage for emplacement new so it is faster when the majority of sizeof objects that get stored in any<> fall under a given value.
 * _common.hpp_: More bit-wise algorithms
 * _dynamictype.hpp_: Alternative support to dynamic_cast
 * _dynamicpointer.hpp_: Smart pointer of "dynamic" objects - objects with alternative support to dynamic_cast.
 * _event.hpp_: Definition of an Event object, a combination between something like std::error_code and an solid::Any<>.
 * _innerlist.hpp_: A container wrapper which allows implementing bidirectional lists over a std::vector/std::deque (extensively used by the solid_frame_ipc library).
 * _memoryfile.hpp_: A data store with file like interface.
 * _queue.hpp_: An alternative to std::queue
 * _stack.hpp_: An alternative to std::stack
 * _workpool.hpp_: Generic thread pool.
 
### solid_serialization
 * _binary.hpp_: Binary "asynchronous" serializer/deserializer.
 * _binarybasic.hpp_: Some "synchronous" load/store functions for basic types.
 * _typeidmap.hpp_: Class for helping "asynchronous" serializer/deserializer support polymorphism: serialize pointers to base classes.
 
The majority of serializers/deserializers offers the following functionality:
 * Synchronously serialize a data structure to a stream (e.g. std::ostringstream)
 * Synchronously deserialize a data structure from a stream (e.g. std::istringstream)

 This means that at a certain moment, one will have the data structure twice in memory: the initial one and the one from the stream.
 The __solid_serialization__ library takes another, let us call it "asynchronous" approach. In solid_serialization the marshaling is made in two overlapping steps:
  * Push data structures into serialization/deserialization engine. No serialization is done at this step. The data structure is split into sub-parts known by the serialization engine and scheduled for serialization.
  * Marshaling/Unmarshaling
   * Marshaling: Given a fixed size buffer buffer (char*) do:
    * call serializer.run to fill the buffer
    * do something with the filled buffer - write it on socket, on file etc.
    * loop until serialization finishes.
   * Unmarshaling: Given a fixed size buffer (char*) do:
    * fill the buffer with data from a file/socket etc.
    * call deserializer.run with the given data
    * loop until deserialization finishes

This approach allows serializing data that is bigger than the system memory - e.g. serializing a data structure containing a file stream (see [ipc file tutorial](tutorials/ipc_file) especially [messages definition](tutorials/ipc_file/ipc_file_messages.hpp)).
 
### solid_frame

The library offers the base support for an asynchronous active object model and implementation for objects with basic support for notification and timer events.

 * _manager.hpp_: A synchronous, passive store of ObjectBase grouped by services.
 * _object.hpp_: An active object with support for events: notification events and timer events.
 * _objectbase.hpp_: Base for all active Objects
 * _reactor.hpp_: An active store of Objects with support for notification events and timer events.
 * _reactorbase.hpp_: Base for all reactors
 * _reactorcontext.hpp_: A context class given as parameter to every callback called from the reactor.
 * _scheduler.hpp_: A generic pool of threads running reactors.
 * _schedulerbase.hpp_: Base for all schedulers.
 * _service.hpp_: A way of grouping related objects.
 * _sharedstore.hpp_: A store of shared object with synchronized non-conflicting read/read-write access.
 * _timer.hpp_: Used by Objects needing timer events.
 * _timestore.hpp_: Used by reactors for timer events support.
 
[Here](solid/frame/README.md) you can find an overview of the asynchronous active object model employed by the solid_frame framework.

### solid_frame_aio

The library extends solid_frame with active objects supporting IO, notification and timer events.
 * _aiodatagram.hpp_: Used by aio::Objects to support asynchronous UDP communication.
 * _aiostream.hpp_: Used by aio::Objects to support asynchronous TCP communication.
 * _aiotimer.hpp_: Used by aio::Objects needing timer events.
 * _aiolistener.hpp_: Used by aio::Objects listening for TCP connections.
 * _aioreactor.hpp_: An active store of aio::Objects with support for IO, notification and timer events.
 * _aiosocket.hpp_: Plain socket access used by Listener/Stream and Datagram
 * _aioresolver.hpp_: Asynchronous address resolver.
 * _aioreactorcontext.hpp_: A context class given as parameter to every callback called from the aio::Reactor.
 
[Here](solid/frame/README.md) you can find an overview of the asynchronous active object model employed by the solid_frame framework.

### solid_frame_aio_openssl

The library extends solid_frame_aio with support for Secure Sockets.
 * _aiosecuresocket.hpp_: Used by aio::Stream for SSL.
 * _aiosecurecontext.hpp_: OpenSSL context wrapper.


### solid_frame_ipc

Inter Process Communication library via Plain/Secure TCP connections and a protocol based on solid_serialization.

 * _ipcservice.hpp_: Main interface of the library. Sends ipc::Messages to different recipients and receives ipc::Messages.
 * _ipcmessage.hpp_: Base class for all messages sent through ipc::Service.
 * _ipccontext.hpp_: A context class given to all callbacks called by the ipc library.
 * _ipcconfiguration.hpp_: Configuration data for ipc::Service.


### solid_frame_file

The library offers a specialization of frame::ShareStore for files.

 * _filestore.hpp_: specialization of frame::SharedStore for files with support for temporary files.
 * _filestream.hpp_: std::stream support
 * _tempbase.hpp_: Base class for temporary files: either in memory or disk files.
