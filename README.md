# SolidFrame

Cross-platform C++ framework for asynchronous, distributed applications.

## Build status
|||
|:---|----|
|master|[![Build Status master](https://travis-ci.org/solidoss/solidframe.svg?branch=master)](https://travis-ci.org/solidoss/solidframe)|
|work|[![Build Status work](https://travis-ci.org/solidoss/solidframe.svg?branch=work)](https://travis-ci.org/solidoss/solidframe)|

## Copyright

Copyright (c) 2007-present Valentin Palade (vipalade @ gmail.com).

All rights reserved.

## License

Boost Software License - Version 1.0 - August 17th, 2003

## Resources
 * [Tutorials](tutorials/README.md)
 * [Release Notes](RELEASES.md)
 * [MPRPC library](solid/frame/mprpc/README.md)
 * [API Reference](solid/API.md) - __TODO__
 * [Wiki](https://github.com/vipalade/solidframe/wiki) - __TODO__

## Prerequisites
* C++14 enabled compiler
* [CMake](https://cmake.org/): for build system
* [CxxOpts](https://github.com/jarro2783/cxxopts): needed by SolidFrame examples.
* [OpenSSL](https://www.openssl.org/)/[BoringSSL](https://boringssl.googlesource.com/boringssl/): needed by solid_frame_aio_openssl library.

## Supported platforms

* **Linux** - _gcc_, _llvm_
* **FreeBSD** - _llvm_
* **Darwin/macOS** - _llvm_ - (starting with XCode 8 which has support for thread_local)
* **iOS** - _llvm_ + [CocoaPods](https://cocoapods.org/) - example: [Bubbles](https://github.com/vipalade/bubbles)) 
* **Android** - _llvm/gcc_ - (starting with Android Studio 2.2 - example: [Bubbles](https://github.com/vipalade/bubbles))
* **Windows** - MSVC - tested on Windows 10 with Visual Studio 2017

## Libraries

**Focused:**

* [__solid_frame_mprpc__](#solid_frame_mprpc): Message Passing - Remote Procedure Call over secure/plain TCP ([MPRPC library](solid/frame/mprpc/README.md))
    * _mprpc::Service_ - pass messages to/from multiple peers.
* [__solid_frame_aio__](#solid_frame_aio): asynchronous communication library using epoll on Linux and kqueue on FreeBSD/macOS
    * _Object_ - reactive object with support for Asynchronous IO
    * _Reactor_ - reactor with support for Asynchronous IO
    * _Listener_ - asynchronous TCP listener/server socket
    * _Stream_ - asynchronous TCP socket
    * _Datagram_ - asynchronous UDP socket
    * _Timer_ - allows objects to schedule time based events
* [__solid_serialization_v2__](#solid_serialization_v2): binary serialization/marshaling
    * _TypeMap_
    * _binary::Serializer_
    * _binary::Deserializer_

**All:**

* [__solid_system__](#solid_system):
    * Wrappers for socket/file devices, socket address, directory access
    * Log engine
* [__solid_utility__](#solid_utility):
    * _Any_ - similar to boost::any
    * _Event_ - Event class containing an ID a solid::Any object and a Category (similar to std::error_category)
    * _InnerList_ - bidirectional list mapped over a vector/deque
    * _Stack_ - alternative to std::stack
    * _Queue_ - alternative to std:queue
    * _WorkPool_ - generic thread pool
* [__solid_serialization_v2__](#solid_serialization_v2): binary serialization/marshalling
    * _TypeMap_
    * _binary::Serializer_
    * _binary::Deserializer_
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
* [__solid_frame_mprpc__](#solid_frame_mprpc): Message Passing - Remote Procedure Call over TCP ([MPRPC library](solid/frame/mprpc/README.md))
    * _mprpc::Service_ - pass messages to/from multiple peers.

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
 * C++14 enabled compiler: gcc-c++ on Linux and clang on FreeBSD and macOS (minimum: XCode 8/Clang 8).
 * [CMake](https://cmake.org/)

Bash commands for installing _SolidFrame_:

```bash
$ mkdir ~/work
$ cd ~/work
$ git clone git@github.com:vipalade/solidframe.git
$ mkdir external
$ cd extern
$ ../solidframe/prerequisites/build.sh
# ... wait until the prerequisites are built
$ cd ../solidframe
$ ./configure -e ~/work/external --prefix ~/work/external
$ cd build/release
$ make install
# ... when finished, the header files will be located in ~/work/extern/include/solid
# and the libraries at ~/work/external/lib/libsolid_*.a
```

If debug build is needed, the configure line will be:

```bash
$ ./configure -b debug -e ~/work/external --prefix ~/work/external
$ cd build/debug
```

Also if, on Linux clang toolchain is wanted for build, the configure line will be:

```bash
$ CXX=clang++ CC=clang ./configure -e ~/work/external --prefix ~/work/external
```

For more information about ./configure script use:

```bash
$ ./configure --help
```

### Android

**SolidFrame** can be intergrated into any Android project by enabling C++ and CMake support and build **SolidFrame** from the project's CMakeLists.txt file, either as a git submodule or as CMake external project.

[Bubbles](https://github.com/vipalade/bubbles/tree/master/client/android/bubbles) Android Studio project, exemplifies how to integrate **SolidFrame** as a git submodule.

The [CMakeLists.txt](https://github.com/vipalade/bubbles/blob/master/client/android/bubbles/app/CMakeLists.txt) file also exemplifies how to use [Google Snappy](https://github.com/google/snappy) library as an ExternalProject which can be used as a starting point for how to integrate **SolidFrame** as a CMake ExternalProject too. 

### iOS

**SolidFrame** can be integrated into any iOS (Swift) project as an [Cocoa Pod](https://cocoapods.org/) by adding lines as the following, to the project's Podfile:

```Podfile
  pod 'SolidFrame/frame_mprpc', :configuration => 'Debug', :git => 'https://github.com/vipalade/solidframe.git', :branch => 'master'
  pod 'SolidFrame/serialization_v2', :configuration => 'Debug', :git => 'https://github.com/vipalade/solidframe.git', :branch => 'master'
  pod 'SolidFrame/frame_aio_openssl', :configuration => 'Debug', :git => 'https://github.com/vipalade/solidframe.git', :branch => 'master'
```

[Bubbles](https://github.com/vipalade/bubbles/tree/master/client/ios/bubbles) is a sample XCode Swift CocoaPod enabled project using **SolidFrame**.

### Windows

System prerequisites:
 * [Visual Studio 2017](https://www.visualstudio.com/) - e.g. Community Edition.
 * [CMake](https://cmake.org/)
 * [Git for Windows](https://git-scm.com/download/win) - the build flow uses Git Bash
 * [Perl for Windows](http://strawberryperl.com/) - needed for building OpenSSL. Windows Git Bash installation also comes with __perl__ but it won't work with OpenSSL build.

The Windows build flow, only relies on the prebuilt external folder of dependencies (on Linux/macOS/FreeBSD it is also available a build flow based on cmake's ExternalProject_Add, which automatically downdloads and builds the external prerequisites). To prepare the external folder one should use the prerequisites/build.sh script from Git Bash console as described below:

```bash
$ mkdir ~/work
$ cd ~/work
$ git clone https://github.com/vipalade/solidframe.git
$ mkdir external
$ cd external
$ ../solidframe/prerequisites/run_in_visual_studio_2017_environment.sh amd64 bash
# a new Git Bash instance will be created with Visual Studio 2017 environment setup.
# build OpenSSL
$ ../solidframe/prerequisites/build.sh --64bit
$ cd ../solidframe
$ ./configure -b release -f vsrls64 -e ~/work/external -g "Visual Studio 15 2017 Win64"
$ cd build/vsrls64
# the current folder contains SolidFrame.sln solution which can be opened in Visual Studio 2017
#
# issue a full build from from command line
$ cmake --build . --config release
# the following command can be used on build only the libraries:
# cmake --build . --config release --target libraries
#
# command used for running the tests:
$ cmake --build . --config release --target RUN_TESTS
``` 

#### Use SolidFrame in your projects

_With CMake - the recommended way:_

In CMakeLists.txt add something like:

```CMake
set(SolidFrame_DIR "${EXTERNAL_DIR}/lib/cmake/SolidFrame" CACHE PATH "SolidFrame CMake configuration dir")
find_package(SolidFrame)
```

Where EXTERNAL_DIR points to where SolidFrame was installed (e.g. ~/work/external).

You can also directly use SolidFrame libraries from SolidFrame's build directory by specifying the SolidFrame_DIR when running cmake for your project:

```bash
$ cd my_project/build
$ cmake -DEXTERNAL_DIR=~/work/external -DSolidFrame_DIR=~/work/solidframe/build/release -DCMAKE_BUILD_TYPE=debug ..
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

## Maintainance

SolidFrame's build system has built-in support for:
 * code reformatting via __clang-format__
 * code static analisys via __clang-tidy__

### source reformating via clang-format

For now the support is only available on Linux systems where "clang-format" package should be installed.

There is a "__code-auto-format__" build target which will scan and reformat all source code files using clang-format, configured via [.clang-format](.clang-format) file.

### static analisys via clang-tidy

For now the support is only available on Linux systems where "clang-tidy" package should be installed.

The static analisys via clang-tidy is intergrated into the cmake build system.
I can be enabled by setting "CMake_RUN_CLANG_TIDY" boolean via __cmake-gui__ after a build configuration was created, or by specifing as parameter when creating a build configuration:

```bash
./configure -b debug -e ~/work/external -P "-DCMake_RUN_CLANG_TIDY:BOOLEAN=true"
```
__clang-tidy__ is controlled via [.clang-tidy](.clang-tidy) configuration file.

## Overview

_SolidFrame_ is an experimental framework to be used for implementing cross-platform C++ network enabled applications or modules.

The consisting libraries only depend on C++ STL with one exception:
 * __solid_frame_aio_openssl__: which depends on either [OpenSSL](https://www.openssl.org/) or [BoringSSL](https://boringssl.googlesource.com/boringssl/)

In order to keep the framework as dependency-free as possible some components that can be found in other libraries/frameworks were re-implemented here too, most of them being locate in either __solid_utility__ or __solid_system__ libraries.

The next paragraphs will briefly present every library.

### <a id="solid_system"></a>solid_system

The library consists of wrappers around system calls for:

 * [__socketaddress.hpp__](solid/system/socketaddress.hpp): socket addresses, synchronous socket address resolver
 * [__nanotime.hpp__](solid/system/nanotime.hpp): high precision clock
 * [__log.hpp__](solid/system/log.hpp): log engine
 * [_device.hpp_](solid/system/device.hpp), [_seekabledevice.hpp_](solid/system/seekabledevice.hpp), [_filedevice.hpp_](solid/system/filedevice.hpp), [_socketdevice.hpp_](solid/system/socketdevice.hpp): devices (aka descriptors/handles) for: files, sockets etc.
 * [_directory.hpp_](solid/system/directory.hpp): basic file-system directory operations

The most notable component is the log engine which allows sending log records to different locations:
  * file (with support for rotation)
  * stderr/stdlog
  * socket endpoint

The log engine defines two macros for specify log lines:
  * __solid_log__: always consider logging the given log line
  * __solid_dbg__: consider logging the line only on debug builds or when SOLID_HAS_DEBUG is defined (otherwise, the macros are empty - i.e. no code is generated)


```C++
    solid::log_start(std::cerr, {".*:VIEWS"});
    //...
    //a log line on the generic logger
    solid_log(generic_logger, Info, "starting aio server scheduler: "<<err.message());
    //...
    //a debug log line on a given logger:
    solid_dbg(logger, Verbose, this<<" enqueue message "<<_rpool_msg_id<<" to connection "<<this<<" retval = "<<success);
```

the "logger" in the above code is usually created like this:
```C++
namespace{
    const LoggerT logger("solid::frame::aio::openssl");
}
```

Notes on the above two blocks of code:
 * The last parameter for log_start is a list of strings with the format [Regular Expression]:[V|I|E|W|S|v|i|e|w|s]
    * the regular expression is used for matching logger names
    * the flag part is used for enabling (the upper letters) or disabling (the lower letters) log levels.
 * The meaning of letters that can be used in the flag part, correnspond to log levels:
    * V|v: Verbose
    * I|i: Info
    * E|e: Error
    * W|w: Warning
    * S|s: Statistics


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
AnyT    a;

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
    uint64_t    a;
    uint64_t    b;
    double      c;
};

struct B{
    uint64_t    a;
    uint64_t    b;
    double      c;
    char        buf[64];
};

struct C{
    uint64_t    a;
    uint64_t    b;
    double      c;
    char        buf[128];
};

AnyT    a;
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
const AlphaEventCategory    alpha_event_category{
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

### <a id="solid_serialization_v2"></a>solid_serialization_v2
 * [_typemap.hpp_](solid/serialization/v2/typemap.hpp): Enable polimorphism support in serializers/deserializers.
 * [_binaryserializer.hpp_](solid/serialization/v2/binaryserializer.hpp): Binary "asynchronous" serializer.
 * [_binarydeserializer.hpp_](solid/serialization/v2/binarydeserializer.hpp): Binary "asynchronous" deserializer.
 * [_binarybasic.hpp_](solid/serialization/v2/binarybasic.hpp): Some "synchronous" load/store functions for basic types.
 * [_typeidmap.hpp_](solid/serialization/typeidmap.hpp): Class for helping "asynchronous" serializer/deserializer support polymorphism: serialize pointers to base classes.

The majority of serializers/deserializers offers the following functionality:
 * Synchronously serialize a data structure to a stream (e.g. std::ostringstream)
 * Synchronously deserialize a data structure from a stream (e.g. std::istringstream)

This means that at a certain moment, one will have the data structure twice in memory: the initial one and the one from the stream.

The first version of the solid serialization library had two distinct steps for serilization:
 * schedule items for serialization in a stack like callback structure
 * do the actual item serialzation/deserialization when output space or input data is available
 
In order to improve speed, the second version of the library tries to overlap the above two steps - i.e. the items are scheduled only if the serialization/deserialization cannot be done inplace - e.g. the buffer is either full or empty. 

Notable are two other abilities of the serialization engine:
 * The support serializing streams - see [ipc file tutorial](tutorials/mprpc_file) especially [messages definition](tutorials/mprpc_file/mprpc_file_messages.hpp)
 * The support for imposing limits on items: string, container, stream - i.e. serialization/deserialization terminates with an error if an item exceeds the limit set for the item category (either string, container, stream). This is very important when usend in an online protocol.


[Sample code](solid/serialization/v2/test/test_binary.cpp)

A C++ structure with serialization support:

```C++
struct Test{
    using KeyValueVectorT = std::vector<std::pair<std::string, std::string>>;
    using MapT = std::map<std::string, uint64_t>;

    std::string         str;
    KeyValueVectorT     kv_vec;
    MapT                kv_map;
    uint32_t            v32;

    SOLID_SERIALIZE_V2(_s, _rthis, _rctx, _name){
        _s.add(str, _rctx, "Test::str");
        _s.add(kv_vec, _rctx, "Test::kv_vec").add(kv_map, _rctx, "Test::kv_map");
        _s.add(v32, _rctx, "Test::v32");
    }
};
```

Defining the typemap/serializer/deserializer:

```C++
#include "solid/serialization/serialization.hpp"

struct Context{};
struct TypeData{};

using TypeIdT       = uint8_t;//the type that is serialized and used to identify the registered type.
using TypeMapT      = serialization::TypeIdMap<TypeIdT, Context, solid::serialization::Serializer, solid::serialization::Deserializer, TypeData>;
using SerializerT   = TypeMapT::SerializerT;
using DeserializerT = TypeMapT::DeserializerT;
```

Prepare the typemap:

```C++
TypeMapT  typemap;
typemap.null(0);//null typeid
typemap.registerType<Test>(1/*type id*/);
```

Serialize and deserialize a Test structure:
```C++
std::string     data;
{//serialize
    Context         ctx;
    SerializerT     ser = typemap.createSerializer();
    const int       bufcp = 64;
    char            buf[bufcp];
    int             rv;

    std::shared_ptr<Test>   test_ptr = Test::create();
    test_ptr->init();

    ser.add(test_ptr, ctx, "test_ptr");

    while((rv = ser.run(buf, bufcp, ctx)) > 0){
        data.append(buf, rv);
    }
}
{//deserialize
    Context                 ctx;
    DeserializerT           des = typemap.createDeserializer();

    std::shared_ptr<Test>   test_ptr;

    des.add(test_ptr, ctx, "test_ptr");

    size_t                  rv = des.run(data.data(), data.size(), ctx);
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

Work in progress: The library extends solid_frame_aio with support for Secure Sockets based on OpenSSL/BoringSSL libraries.
 * [__aiosecuresocket.hpp__](solid/frame/aio/openssl/aiosecuresocket.hpp): Used by aio::Stream for SSL.
 * [__aiosecurecontext.hpp__](solid/frame/aio/openssl/aiosecurecontext.hpp): OpenSSL context wrapper.


### <a id="solid_frame_mprpc"></a>solid_frame_mprpc

Message Passing - Remote Procedure Call library:
 * Pluggable - i.e. header only - protocol based on solid_serialization.
 * Pluggable - i.e. header only - support for secure communication via solid_frame_aio_openssl.
 * Pluggable - i.e. header only - support for communication compression via Snappy.

The header only plugins ensure that solid_frame_mprpc itself does not depend on the libraries the plugins depend on.


 * [__mprpcservice.hpp__](solid/frame/mprpc/mprpcservice.hpp): Main interface of the library. Sends mprpc::Messages to different recipients and receives mprpc::Messages.
 * [__mprpcmessage.hpp__](solid/frame/mprpc/mprpcmessage.hpp): Base class for all messages sent through mprpc::Service.
 * [__mprpccontext.hpp__](solid/frame/mprpc/mprpccontext.hpp): A context class given to all callbacks called by the mprpc library.
 * [__mprpcconfiguration.hpp__](solid/frame/mprpc/mprpcconfiguration.hpp): Configuration data for mprpc::Service.

__Usefull links__
 * [MPRPC README](solid/frame/mprpc/README.md)
 * [MPRPC Relay](solid/frame/mprpc/README.md#relay_engine)
 * [Tutorial: aio_echo](tutorials/aio_echo/README.md)
 * [Tutorial: mprpc_echo](tutorials/mprpc_echo/README.md)
 * [Tutorial: mprpc_request](tutorials/mprpc_request/README.md)
 * [Tutorial: mprpc_request_ssl](tutorials/mprpc_request_ssl/README.md)
 * [Tutorial: mprpc_file](tutorials/mprpc_file/README.md)
 * [Tutorial: mprpc_echo_relay](tutorials/mprpc_echo_relay/README.md)

### <a id="solid_frame_file"></a>solid_frame_file

The library offers a specialization of frame::ShareStore for files.

 * [__filestore.hpp__](solid/frame/file/filestore.hpp): specialization of frame::SharedStore for files with support for temporary files.
 * [_filestream.hpp_](solid/frame/file/filestream.hpp): std::stream support
 * [_tempbase.hpp_](solid/frame/file/tempbase.hpp): Base class for temporary files: either in memory or disk files.
