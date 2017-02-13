# SolidFrame

Cross-platform C++ framework for asynchronous, distributed applications.

## License

Boost Software License - Version 1.0 - August 17th, 2003

## Resources
 * [Tutorials](tutorials/README.md)
 * [Release Notes](RELEASES.md)
 * [IPC library](solid/frame/ipc/README.md)
 * [API Reference](solid/API.md) - __TODO__
 * [Wiki](https://github.com/vipalade/solidframe/wiki) - __TODO__

## Prerequisites
* C++11 enabled compiler
* [CMake](https://cmake.org/): for build system
* [boost](http://www.boost.org/): for tests, examples and tutorials - not for libraries themselves.
* [OpenSSL](https://www.openssl.org/)/[BorringSSL](https://boringssl.googlesource.com/boringssl/): needed by solid_frame_aio_openssl library.

## Supported platforms

* **Linux** - _gcc_ - (tested on latest Fedora i686/x86_64, Ubuntu LTS and Raspian on Raspberry Pi 2 armv7l)
* **FreeBSD** - _llvm_ - (tested on FreeBSD/PcBSD 10.3)
* **Darwin/macOS** - _llvm_ - (starting with XCode 8 which has support for thread_local)
* **Android** - _llvm/gcc_ - (starting with Android Studio 2.2 - examples: [Bubbles](https://github.com/vipalade/bubbles), [EchoClient](https://github.com/vipalade/study_android/tree/master/EchoClient))
* Windows - MSVC - (partial)

## Libraries

**Focused:**

* [__solid_frame_mpipc__](#solid_frame_mpipc): Message Passing Inter-Process Communication over secure/plain TCP ([MPIPC library](solid/frame/mpipc/README.md))
	* _mpipc::Service_ - pass messages to/from multiple peers.
* [__solid_frame_aio__](#solid_frame_aio): asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
	* _Object_ - reactive object with support for Asynchronous IO
	* _Reactor_ - reactor with support for Asynchronous IO
	* _Listener_ - asynchronous TCP listener/server socket
	* _Stream_ - asynchronous TCP socket
	* _Datagram_ - asynchronous UDP socket
	* _Timer_ - allows objects to schedule time based events
* [__solid_serialization__](#solid_serialization): binary serialization/marshaling
	* _binary::Serializer_
	* _binary::Deserializer_
	* _TypeIdMap_

**All:**

* [__solid_system__](#solid_system):
	* Wrappers for socket/file devices, socket address, directory access
	* Debug log engine
* [__solid_utility__](#solid_utility):
	* _Any_ - similar to boost::any
	* _Event_ - Event class containing an ID a solid::Any object and a Category (similar to std::error_category)
	* _InnerList_ - bidirectional list mapped over a vector/deque
	* _Stack_ - alternative to std::stack
	* _Queue_ - alternative to std:queue
	* _WorkPool_ - generic thread pool
* [__solid_serialization__](#solid_serialization): binary serialization/marshalling
	* _binary::Serializer_
	* _binary::Deserializer_
	* _TypeIdMap_
* [__solid_frame__](#solid_frame):
	* _Object_ - reactive object
	* _Manager_ - store services and notifies objects within services
	* _Service_ - store and notifies objects
	* _Reactor_ - active store of objects - allows objects to asynchronously react on events
	* _Scheduler_ - a thread pool of reactors
	* _Timer_ - allows objects to schedule time based events
	* _shared::Store_ - generic store of shared objects that need either multiple read or single write access
* [__solid_frame_aio__](#solid_frame_aio): asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
	* _Object_ - reactive object with support for Asynchronous IO
	* _Reactor_ - reactor with support for Asynchronous IO
	* _Listener_ - asynchronous TCP listener/server socket
	* _Stream_ - asynchronous TCP socket
	* _Datagram_ - asynchronous UDP socket
	* _Timer_ - allows objects to schedule time based events
* [__solid_frame_aio_openssl__](#solid_frame_aio_openssl): SSL support via OpenSSL
* [__solid_frame_file__](#solid_frame_file)
	* _file::Store_ - a shared store for files
* [__solid_frame_mpipc__](#solid_frame_mpipc): Message Passing InterProcess Communication over TCP ([MPIPC library](solid/frame/mpipc/README.md))
	* _mpipc::Service_ - pass messages to/from multiple peers.

## <a id="installation"></a>Installation

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

If debug build is needed, the configure line will be:

```bash
$ ./configure -b debug -e ~/work/extern --prefix ~/work/extern
$ cd build/debug
```

For more information about ./configure script use:

```bash
$ ./configure --help
```

#### Use SolidFrame in your projects

_With CMake - the recommended way:_

In CMakeLists.txt add something like:

```CMake
set(SolidFrame_DIR "${EXTERN_PATH}/lib/cmake/SolidFrame" CACHE PATH "SolidFrame CMake configuration dir")
find_package(SolidFrame)
```

Where EXTERN_PATH points to where SolidFrame was installed (e.g. ~/work/extern).

You can also directly use SolidFrame libraries from SolidFrame's build directory by specifying the SolidFrame_DIR when running cmake for your project:

```bash
$ cd my_project/build
$ cmake -DEXTERN_PATH=~/work/extern -DSolidFrame_DIR=~/work/solidframe/build/release -DCMAKE_BUILD_TYPE=debug ..
```

_Without CMake:_

You need to specify the location for SolidFrame includes:

```make
-I~/work/extern/include
```
and the location for SolidFrame libraries:

```make
-L~/work/extern/lib
```

### Windows
Windows is not yet supported.

## Overview

_SolidFrame_ is an experimental framework to be used for implementing cross-platform C++ network enabled applications or modules.

The consisting libraries only depend on C++ STL with one exception:
 * __solid_frame_aio_openssl__: which depends on either [OpenSSL](https://www.openssl.org/) or [BorringSSL](https://boringssl.googlesource.com/boringssl/)

In order to keep the framework as dependency-free as possible some components that can be found in other libraries/frameworks were re-implemented here too, most of them being locate in either __solid_utility__ or __solid_system__ libraries.

The next paragraphs will briefly present every library.

### <a id="solid_system"></a>solid_system

The library consists of wrappers around system calls for:
 
 * [__socketaddress.hpp__](solid/system/socketaddress.hpp): socket addresses, synchronous socket address resolver
 * [__nanotime.hpp__](solid/system/nanotime.hpp): high precision clock
 * [__debug.hpp__](solid/system/debug.hpp): debug log engine
 * [_device.hpp_](solid/system/device.hpp), [_seekabledevice.hpp_](solid/system/seekabledevice.hpp), [_filedevice.hpp_](solid/system/filedevice.hpp), [_socketdevice.hpp_](solid/system/socketdevice.hpp): devices (aka descriptors/handles) for: files, sockets etc.
 * [_directory.hpp_](solid/system/directory.hpp): basic file-system directory operations

The most notable component is the debug log engine which allows sending log records to different locations:
  * file (with support for rotation)
  * stderr/stdlog
  * socket endpoint

The debug engine defines some macros for easily specify log lines. The macros are only active (do something) when SolidFrame is compiled with SOLID_HAS_DEBUG (e.g. maintain and debug builds).
Also, the debug engine has support for registering modules and for specifying enabled log levels. Here is an example:

```C++
	Debug::the().levelMask("view");
	Debug::the().moduleMask("frame_ipc:iew any:ew");
	Debug::the().initStdErr();
	//...
	//a log line for module "any"
	edbg("starting aio server scheduler: "<<err.message());
	//...
	//a log line for a predefined module "frame_ipc" aka Debug::ipc
	idbgx(Debug::ipc, this<<" enqueue message "<<_rpool_msg_id<<" to connection "<<this<<" retval = "<<success);
```

In the above code we:
 * Set the global levelMask to "view" (V = Verbose, I = Info, E = Error, W = Warning).
 * Enable logging for only two modules: "frame_mpipc" and "any" (the generic module used by [v/i/e/w]dbg() macros). For "frame_mpipc" restrict the level mask to {Info, Error, Warning} and for "any" restrict it to only {Error and Warning}.
 * Configure the debug log engine to send log records to stderr.
 * Send a log _error_ line for "any" module.
 * Send a log _info_ line for "frame_mpipc" module.

The Debug engine allows for registering new modules like this:

```C++
	static const auto my_module_id = Debug::the().registerModule("my_module");
	//...
	//enable the module 
	Debug::the().moduleMask("frame_mpipc:iew any:ew my_module:view");
	//...
	//log a INFO line for the module:
	idbgx(my_module_id, "error starting engine: "<<error.mesage());
```
or like this:

```C++
	static unsigned my_module_id(){
		static const auto id = Debug::the().registerModule("my_module");
		return id;
	}
	
	//...
	//enable the module 
	Debug::the().moduleMask("frame_mpipc:iew any:ew my_module:view");
	//...
	//log a INFO line for the module:
	idbgx(my_module_id(), "error starting engine: "<<error.mesage());
```

 
### <a id="solid_utility"></a>solid_utility

The library consists of tools needed by upper level libraries:
 * [__any.hpp__](solid/utility/any.hpp): A variation on boost::any / experimental std::any with storage for emplacement new so it is faster when the majority of sizeof objects that get stored in any<> fall under a given value.
 * [__event.hpp__](solid/utility/event.hpp): Definition of an Event object, a combination between something like std::error_code and an solid::Any<>.
 * [__innerlist.hpp__](solid/utility/innerlist.hpp): A container wrapper which allows implementing bidirectional lists over a std::vector/std::deque (extensively used by the solid_frame_ipc library).
 * [__memoryfile.hpp__](solid/utility/memoryfile.hpp): A data store with file like interface.
 * [__workpool.hpp__](solid/utility/workpool.hpp): Generic thread pool.
 * [_dynamictype.hpp_](solid/utility/dynamictype.hpp): Base for objects with alternative support to dynamic_cast
 * [_dynamicpointer.hpp_](solid/utility/dynamicpointer.hpp): Smart pointer to "dynamic" objects - objects with alternative support to dynamic_cast.
 * [_queue.hpp_](solid/utility/queue.hpp): An alternative to std::queue
 * [_stack.hpp_](solid/utility/stack.hpp): An alternative to std::stack
 * [_algorithm.hpp_](solid/utility/algorithm.hpp): Some inline algorithms
 * [_common.hpp_](solid/utility/common.hpp): Some bits related algorithms
 
Here are some sample code for some of the above tools:

__Any__: [Sample code](solid/utility/test/test_any.cpp)

```C++
	using AnyT = solid::Any<>;
	//...
	AnyT	a;
	
	a = std::string("Some text");
	
	//...
	std::string *pstr = a.cast<std::string>();
	if(pstr){
		cout<<*pstr<<endl;
	}
```
Some code to show the difference from boost::any:

```C++
	using AnyT = solid::Any<128>;
	
	struct A{
		uint64_t	a;
		uint64_t	b;
		double		c;
	};
	
	struct B{
		uint64_t	a;
		uint64_t	b;
		double		c;
		char		buf[64];
	};
	
	struct C{
		uint64_t	a;
		uint64_t	b;
		double		c;
		char		buf[128];
	};
	
	AnyT	a;
	//...
	a = std::string("Some text");//as long as sizeof(std::string) <= 128 no allocation is made - Any uses placement new()
	//...
	a = A();//as long as sizeof(A) <= 128 no allocation is made - Any uses placement new()
	//...
	a = B();//as long as sizeof(B) <= 128 no allocation is made - Any uses placement new()
	//...
	a = C();//sizeof(C) > 128 so new allocation is made - Any uses new
	
```

__Event__: [Sample code](solid/utility/test/test_event.cpp)

Create a new event category:

```C++
enum struct AlphaEvents{
	First,
	Second,
	Third,
};
using AlphaEventCategory = EventCategory<AlphaEvents>;
const AlphaEventCategory	alpha_event_category{
	"::alpha_event_category",
	[](const AlphaEvents _evt){
		switch(_evt){
			case AlphaEvents::First:
				return "first";
			case AlphaEvents::Second:
				return "second";
			case AlphaEvents::Third:
				return "third";
			default:
				return "unknown";
		}
	}
};
```

Handle events:

```C++
void Object::handleEvent(Event &&_revt){
	static const EventHandler<void, Object&> event_handler = {
		[](Event &_revt, Object &_robj){cout<<"handle unknown event "<<_revt<< on "<<&_robj<<endl;},
		{
			{
				alpha_event_category.event(AlphaEvents::First),
				[](Event &_revt, Object &_robj){cout<<"handle event "<<_revt<<" on "<<&_robj<<endl;}
			},
			{
				alpha_event_category.event(AlphaEvents::Second),
				[](Event &_revt, Object &_robj){cout<<"handle event "<<_revt<<" on "<<&_robj<<endl;}
			},
			{
				generic_event_category.event(GenericEvents::Message),
				[](Event &_revt, Object &_robj){cout<<"handle event "<<_revt<<"("<<*_revt.any().cast<std::string>()<<") on "<<&_robj<<endl;}
			}
		}
	};
	
	event_handler.handle(_revt, *this);
}
```

Creating events:

```C++
	//...
	rbase.handleEvent(alpha_event_category.event(AlphaEvents::Second));
	rbase.handleEvent(generic_event_category.event(GenericEvents::Message, std::string("Some text")));
	//...
```

__InnerList__: [Sample code](solid/utility/test/test_innerlist.cpp)

__MemoryFile__: [Sample code](solid/utility/test/test_memory_file.cpp)

### <a id="solid_serialization"></a>solid_serialization
 * [_binary.hpp_](solid/serialization/binary.hpp): Binary "asynchronous" serializer/deserializer.
 * [_binarybasic.hpp_](solid/serialization/binarybasic.hpp): Some "synchronous" load/store functions for basic types.
 * [_typeidmap.hpp_](solid/serialization/typeidmap.hpp): Class for helping "asynchronous" serializer/deserializer support polymorphism: serialize pointers to base classes.
 
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


[Sample code](solid/serialization/test/test_binary.cpp)

A structure with serialization support:

```C++
struct Test{
	using KeyValueVectorT = std::vector<std::pair<std::string, std::string>>;
	using MapT = std::map<std::string, uint64_t>;
	
	std::string			str;
	KeyValueVectorT		kv_vec;
	MapT				kv_map;
	uint32_t			v32;
	
	template <class S>
	void serialize(S &_s){
		_s.push(str, "Test::str");
		_s.pushContainer(kv_vec, "Test::kv_vec").pushContainer(kv_map, "Test::kv_map");
		_s.pushCross(v32, "Test::v32");
	}
};
```

Defining the serializer/deserializer/typeidmap:

```C++
#include "solid/serialization/binary.hpp"

using SerializerT	= serialization::binary::Serializer<void>;
using DeserializerT = serialization::binary::Deserializer<void>;
using TypeIdMapT	= serialization::TypeIdMap<SerializerT, DeserializerT>;
```

Prepare the typeidmap:

```C++
TypeIdMapT	typemap;
typemap.registerType<Test>(0/*protocol ID*/);
```

Serialize and deserialize a Test structure:
```C++
	std::string		data;
	{//serialize
		SerializerT		ser(&typeidmap);
		const int		bufcp = 64;
		char 			buf[bufcp];
		int				rv;
		
		std::shared_ptr<Test>	test_ptr = Test::create();
		test_ptr->init();
		
		ser.push(test_ptr, "test_ptr");
		
		while((rv = ser.run(buf, bufcp)) > 0){
			data.append(buf, rv);
		}
	}
	{//deserialize
		DeserializerT			des(&typeidmap);
		
		std::shared_ptr<Test>	test_ptr;
		
		des.push(test_ptr, "test_ptr");
		
		size_t					rv = des.run(data.data(), data.size());
		SOLID_CHECK(rv == data.size());
	}
```


### <a id="solid_frame"></a>solid_frame

The library offers the base support for an asynchronous active object model and implementation for objects with basic support for notification and timer events.

 * [__manager.hpp__](solid/frame/manager.hpp): A synchronous, passive store of ObjectBase grouped by services.
 * [__object.hpp__](solid/frame/object.hpp): An active object with support for events: notification events and timer events.
 * [__reactor.hpp__](solid/frame/reactor.hpp): An active store of Objects with support for notification events and timer events.
 * [__reactorcontext.hpp__](solid/frame/reactorcontext.hpp): A context class given as parameter to every callback called from the reactor.
 * [__scheduler.hpp__](solid/frame/scheduler.hpp): A generic pool of threads running reactors.
 * [__service.hpp__](solid/frame/service.hpp): A way of grouping related objects.
 * [__timer.hpp__](solid/frame/timer.hpp): Used by Objects needing timer events.
 * [__sharedstore.hpp__](solid/frame/sharedstore.hpp): A store of shared object with synchronized non-conflicting read/read-write access.
 * [_reactorbase.hpp_](solid/frame/reactorbase.hpp): Base for all reactors
 * [_timestore.hpp_](solid/frame/timestore.hpp): Used by reactors for timer events support.
 * [_schedulerbase.hpp_](solid/frame/schedulerbase.hpp): Base for all schedulers.
 * [_objectbase.hpp_](solid/frame/objectbase.hpp): Base for all active Objects
 

__Usefull links__
 * [An overview of the asynchronous active object model](solid/frame/README.md)

### <a id="solid_frame_aio"></a>solid_frame_aio

The library extends solid_frame with active objects supporting IO, notification and timer events.
 * [__aiodatagram.hpp__](solid/frame/aio/aiodatagram.hpp): Used by aio::Objects to support asynchronous UDP communication.
 * [__aiostream.hpp__](solid/frame/aio/aiostream.hpp): Used by aio::Objects to support asynchronous TCP communication.
 * [__aiotimer.hpp__](solid/frame/aio/aiotimer.hpp): Used by aio::Objects needing timer events.
 * [__aiolistener.hpp__](solid/frame/aio/aiolistener.hpp): Used by aio::Objects listening for TCP connections.
 * [__aiosocket.hpp__](solid/frame/aio/aiosocket.hpp): Plain socket access used by Listener/Stream and Datagram
 * [__aioresolver.hpp__](solid/frame/aio/aioresolver.hpp): Asynchronous address resolver.
 * [__aioreactorcontext.hpp__](solid/frame/aio/aioreactorcontext.hpp): A context class given as parameter to every callback called from the aio::Reactor.
 * [_aioreactor.hpp_](solid/frame/aio/aioreactor.hpp): An active store of aio::Objects with support for IO, notification and timer events.
 
__Usefull links__
 * [An overview of the asynchronous active object model](solid/frame/README.md)
 * [Tutorial: aio_echo](tutorials/aio_echo/README.md)

### <a id="solid_frame_aio_openssl"></a>solid_frame_aio_openssl

Work in progress: The library extends solid_frame_aio with support for Secure Sockets based on OpenSSL/BorringSSL libraries.
 * [__aiosecuresocket.hpp__](solid/frame/aio/openssl/aiosecuresocket.hpp): Used by aio::Stream for SSL.
 * [__aiosecurecontext.hpp__](solid/frame/aio/openssl/aiosecurecontext.hpp): OpenSSL context wrapper.


### <a id="solid_frame_mpipc"></a>solid_frame_mpipc

Message Passing Inter Process Communication library:
 * Pluggable - i.e. header only - protocol based on solid_serialization.
 * Pluggable - i.e. header only - support for secure communication via solid_frame_aio_openssl.
 * Pluggable - i.e. header only - support for communication compression via Snappy.

The header only plugins ensure that solid_frame_mpipc itself does not depend on the libraries the plugins depend on.


 * [__mpipcservice.hpp__](solid/frame/mpipc/mpipcservice.hpp): Main interface of the library. Sends mpipc::Messages to different recipients and receives mpipc::Messages.
 * [__mpipcmessage.hpp__](solid/frame/mpipc/mpipcmessage.hpp): Base class for all messages sent through mpipc::Service.
 * [__mpipccontext.hpp__](solid/frame/mpipc/mpipccontext.hpp): A context class given to all callbacks called by the mpipc library.
 * [__mpipcconfiguration.hpp__](solid/frame/mpipc/mpipcconfiguration.hpp): Configuration data for mpipc::Service.

__Usefull links__
 * [README](solid/frame/mpipc/README.md)
 * [Tutorial: mpipc_echo](tutorials/mpipc_echo/README.md)
 * [Tutorial: mpipc_file](tutorials/mpipc_file/README.md)
 * [Tutorial: mpipc_request](tutorials/mpipc_request/README.md)

### <a id="solid_frame_file"></a>solid_frame_file

The library offers a specialization of frame::ShareStore for files.

 * [__filestore.hpp__](solid/frame/file/filestore.hpp): specialization of frame::SharedStore for files with support for temporary files.
 * [_filestream.hpp_](solid/frame/file/filestream.hpp): std::stream support
 * [_tempbase.hpp_](solid/frame/file/tempbase.hpp): Base class for temporary files: either in memory or disk files.
