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

This approach allows serializing data that is bigger than the system memory - e.g. serializing a data structure containing a file stream (see [ipc file tutorial](tutorials/ipc_file) especially [ipc file tutorial](tutorials/ipc_file/ipc_file_messages.hpp)).
 
 
### solid_frame
### solid_frame_aio
### solid_frame_aio_openssl
### solid_frame_ipc

In the following paragraphs will present some key aspects of the framework:
 * The asynchronous, active objects model
 * The binary serialization library
 * The IPC message passing library

#### The asynchronous, active objects model

When implementing network enabled asynchronous applications one ends up having multiple objects (connections, relay nodes, listeners etc) with certain needs:
 * be able to react on IO events
 * be able to react on timer events
 * be able to react on custom events
	
Let us consider some examples:

__A TCP Connection__
 * handles IO events for its associated socket
 * handles timer event for IO operations - e.g. most often we want to do something if a send operation is taking too long
 * handles custom events - e.g. force_kill when we want a certain connection to forcefully stop.
	
__A TCP Listener__
 * handles IO events for its associated listener socket - new incoming connection
 * handles custom events - e.g. stop listening, pause listening

Let us delve more into a hypothetical TCP connection in a server side scenario.
One of the first actions on a newly accepted connection would be to authenticate the entity (the user) on which behalf the connection was established.
The best way to design the authentication module is with a generic asynchronous interface.
Here is some hypothetical code:

```C++
void Connection::onReceiveAuthentication(Context &_rctx, const std::string &auth_credentials){
	authentication::Service &rauth_service(_rctx.authenticationService());
	
	rauth_service.asyncAuthenticate(
		auth_credentials,
		[/*something*/](std::shared_ptr<UserStub> &_user_stub_ptr, const std::error_condition &_error){
			if(_error){
				//use /*something*/ to notify "this" connection that the authentication failed
			}else{
				//use /*something*/ to notify "this" connection that the authentication succeed
			}
		}
	);
}
```

Before deciding what we can use for /*something*/ lets consider the following constraints:
 * the lambda expression might be executed on a different thread than the one handling the connection.
 * when the asynchronous authentication completes and the lambda is called the connection might not exist - the client closed the connection before receiving the authentication response.

Because of the second constraint, we cannot use a naked pointer to connection (/*something*/ = this), but we can use a std::shared_ptr<Connection>.
The problem is that, then, the connection should have some synchronization mechanism in place (not very desirable in an asynchronous design).

SolidFrame's solution for this is a temporally unique run-time ID for objects. Every object derived from either solid::frame::Object or solid::frame::aio::Object has associated such a unique ID which can be used to notify those objects with events.

Closely related to either Objects are:
 * _solid::frame::Manager_: Passive, synchronized container of registered objects. The Objects are stored grouped by services. It allows sending notification events to specific objects identified by their run-time unique ID.
 * _solid::frame::Service_: Group of objects conceptually related. It allows sending notification events to all registered objects withing the service.
 * _solid::frame::Reactor_: Active container of solid::frame::Objects. Delivers timer and notification events to registered objects.
 * _solid::frame::aio::Reactor_: Active container of solid::frame::aio::Objects. Delivers IO, timer and notification events to registered objects.
 * _solid::frame::Scheduler<ReactorT>_: A thread pool of reactors.
	
Let us look further to some sample code to clarify the use of the above classes:
```C++
int main(int argc, char *argv[]){
	using namespace solid;
	using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

	frame::ObjectIdT	listeneruid;
	
	AioSchedulerT		scheduler;
	frame::Manager		manager;
	frame::Service		service(manager);
	ErrorConditionT		error = scheduler.start(1/*a single thread*/);
	
	if(error){
		cout<<"Error starting scheduler: "<<error.message()<<endl;
		return 1;
	}
	
	{
		ResolveData		rd =  synchronous_resolve("0.0.0.0", listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
		SocketDevice	sd;
			
		sd.create(rd.begin());
		sd.prepareAccept(rd.begin(), 2000);
			
		if(sd.ok()){
			DynamicPointer<frame::aio::Object>	objptr(new Listener(service, scheduler, std::move(sd)));
				
			listeneruid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
			
			if(error){
				SOLID_ASSERT(listeneruid.isInvalid());
				cout<<"Error starting object: "<<error.message()<<endl;
				return 1;
			}
			(void)objuid;
		}else{
			cout<<"Error creating listener socket"<<endl;
			return 1;
		}
	}
	
	//...
	//send listener a dummy event
	if(
		not manager.notify(listeneruid, generic_event_category.event(GenericEvents::Message, std::string("Some ignored message")))
	){
		cout<<"Message not delivered"<<endl;
	}
	
	
	manager.stop();
	return 0;
}
```
Basically the above code instantiates a TCP Listener, starts it and notifies it with a Message event. For the Listener to function it needs a "manager", a "service" and a "scheduler". 

The line:
```C++
	ErrorConditionT		error = scheduler.start(1/*a single thread*/);
```
tries to start the scheduler with a single thread and implicitly a single reactor.

The following lines:
```C++
	ResolveData		rd =  synchronous_resolve("0.0.0.0", listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
	SocketDevice	sd;
		
	sd.create(rd.begin());
	sd.prepareAccept(rd.begin(), 2000);
```
create and configures a socket device/descriptor for listening for TCP connections.
After this, if we have a valid socket device, we need to create and start a Listener object:

```C++
if(sd.ok()){
	DynamicPointer<frame::aio::Object>	objptr(new Listener(service, scheduler, std::move(sd)));
		
	listeneruid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
	
	if(listeneruid.isInvalid()){
		cout<<"Error starting object: "<<error.message()<<endl;
		return 1;
	}
	(void)objuid;
}
```

As you can see above, the Listener constructor needs:
 * service: every accepted connection will be registered onto given service.
 * scheduler: every accepted connection will be scheduled onto given scheduler.
 * sd: the socket device used for listening for new TCP connections.
 
 The next line:
 
 ```C++
	listeneruid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
 ```
 
 will try to atomically:
 * register the Listener object onto service
 * schedule the Listener object onto scheduler along with an initial event
 
 Every object must override:
 
 ```C++
	virtual void Object::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent);
 ```
 
 to receive the events, so on the above code, once the Listener got started, Listener::onEvent will be called on the scheduler thread with the GenericEvents::Start event.
 What will Listener do on onEvent will see later, let us now stay a little bit more on the scheduler.startObject line.
 
 As we can see it returns a frame::ObjectIdT and an error. The error value is obvious so let us see what with the frame::ObjectIdT value. The returned value is the temporally unique run-time ID explained above.
 
 This "listeneruid" can be used at any time during the lifetime of "manager" to notify the Listener object with a custom event, as we do with the following line:
 
 ```C++
	manager.notify(listeneruid, generic_event_category.event(GenericEvents::Message, std::string("Some ignored message")))
 ```
 
**Notes:**
  * One can easily forge a valid ObjectIdT and be able to send an event to a valid Object. This problem will be addressed by future versions of SolidFrame.
  * The object ObjectIdT addresses may not exist when manager.notify is called.
  * Once manager.notify returned true the event will be delivered to the Object.
  * ```generic_event_category.event(GenericEvents::Message, std::string("Some ignored message")``` constructs a generic Message event and instantiates the "any" value contained by the event with a std::string. On the receiving side, the any value can only be retrieved using event.any().cast<std::string>() which returns a pointer to std::string.

Now that you have had a birds eye view of Object:Manager:Service:Scheduler architecture, let us go back to  Connection::onReceiveAuthentication hypothetical code rewrite it with SolidFrame concepts:

```C++
void Connection::onReceiveAuthentication(Context &_rctx, const std::string &auth_credentials){
	
	//NOTE: frame::Manager must outlive authentication::Service
	
	authentication::Service &rauth_service(_rctx.authenticationService());
	frame::Manager		&rmanager(_rctx.service().manager());
	frame::ObjectIdT	connection_id = service(_rctx).manager().id(*this);
	
	rauth_service.asyncAuthenticate(
		auth_credentials,
		[connection_id, &rmanager](std::shared_ptr<UserStub> &_user_stub_ptr, const std::error_condition &_error){
			rmanager.notify(connection_id, Connection::createAuthenticationResultEvent(_user_stub_ptr, _error));
		}
	);
}
```

With the above implementation all that the lambda function does, is to forward the given parameters as a notification event to the object identified by connection_id. We do not care if the object exists.

Now, you might be wondering what is needed on the connection side to create and to handle the authentication result event.

For simple cases where we have few notification events we can use a generic event:
```C++
using AuthenticationResultT = std::pair<std::shared_ptr<UserStub>, std::error_condition>;
/*static*/ Event Connection::createAuthenticationResultEvent(std::shared_ptr<UserStub> &_user_stub_ptr, const std::error_condition &_error){
	return 	generic_event_category.event(GenericEvents::Message, AuthenticationResultT(_user_stub_ptr, _error));
}
```

and do the dispatch using solid::Any<>::cast:

```C++
void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(_revent == generic_event_category.event(GenericEvents::Message)){
		AuthenticationResultT *pauth_result = _revent.any().cast<AuthenticationResultT>();
		if(pauth_result){
			if(pauth_result->second){
				//authentication failed
			}else{
				//authentication succeeded
			}
		}
	}
}
```

If the Connection must handle multiple notification events there is another way of implementing things with a little bit more code.

First we need to create an EventCategory:

```C++
enum class ConnectionEvents{
	//...
	AuthenticationResponse,
	//
};
const EventCategory<ConnectionEvents>	connection_event_category{
	"project::protocol::connection_event_category",
	[](const ConnectionEvents _e){
		switch(_e){
			//...
			case ConnectionEvents::AuthenticationResponse: return "AuthenticationResponse";
			//...
			default: return "Unknown";
		}
	}
};
```

then we need to use it in createAuthenticationResultEvent:

```C++
using AuthenticationResultT = std::pair<std::shared_ptr<UserStub>, std::error_condition>;
/*static*/ Event Connection::createAuthenticationResultEvent(std::shared_ptr<UserStub> &_user_stub_ptr, const std::error_condition &_error){
	return 	connection_event_category.event(ConnectionEvents::AuthenticationResponse, AuthenticationResultT(_user_stub_ptr, _error));
}
```

last, we need to use an event handler in the Connection's onEvent method, like this:

```C++
void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	static const EventHandler<
		void, 
		Connection&,
		frame::aio::ReactorContext&
	>	event_handler = {
		[](Event &_re, Connection &_rcon, frame::aio::ReactorContext &_rctx){
			//do nothing for unknown events
		},
		{
			{
				generic_event_category.event(GenericEvents::Start),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventStart(_rctx, _revt);
				}
			},
			{
				generic_event_category.event(GenericEvents::Kill),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventKill(_rctx, _revt);
				}
			},
			//...
			{
				connection_event_category.event(ConnectionEvents::AuthenticationResponse),
				[](Event &_revt, Connection &_rcon, frame::aio::ReactorContext &_rctx){
					_rcon.doHandleEventAuthenticationResponse(_rctx, _revt);
				}
			},
			//...
		}
	};
	
	//use the static event_handler to dispatch the event:
	event_handler.handle(_revent, *this, _rctx);
}
```

