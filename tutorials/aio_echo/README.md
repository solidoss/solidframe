# solid::frame::aio echo tutorial

Exemplifies the use of solid_frame_aio and solid_frame libraries.

__Source files__:
 * [aio_echo_server.cpp](aio_echo_server.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous actor model](../../solid/frame/README.md).

## Overview

In this tutorial you will learn how to create a simple echo server for both TCP and UDP using solid_frame_aio library. The application we'll be creating will be listening on two different ports:
 * on one port for new TCP connections
 * on another port for UDP datagrams.

For now the tutorial only applies to Linux, macOS and FreeBSD environments.

## Environment

We will consider further that you have a ~/work folder which we will use and that it contains an "extern" folder with SolidFrame's headers and libraries. Here are some bash command lines to verify if everything is ok:

```bash
$ cd ~/work
$ ls extern/include/solid
frame  serialization  solid_config.hpp  system  utility
$ ls extern/lib/libsolid_*
extern/lib/libsolid_frame.a      extern/lib/libsolid_frame_aio_openssl.a  extern/lib/libsolid_frame_ipc.a      extern/lib/libsolid_system.a
extern/lib/libsolid_frame_aio.a  extern/lib/libsolid_frame_file.a         extern/lib/libsolid_serialization.a  extern/lib/libsolid_utility.a
```
If the above BASH commands show similar results then everything is OK and we can move further, otherwise please revisit the [SolidFrame installation](../../README.md#installation).

Now let us create a folder for where the tutorial will reside:

```bash
$ mkdir -p solid_frame_tutorials/aio_echo
```

this is where you will create a file aio_echo_server.cpp which will contain the source code for the servers.

## Compile

The best way to build the tutorial is through a CMake project but as it is outside of this tutorial, we'll be using the plain ol' command line:

```bash
$ cd solid_frame_tutorials/aio_echo
$ c++ -o echo_server aio_echo_server.cpp -I ~/work/extern/include/ -L ~/work/extern/lib  -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## The code

Let us start with the include part and with the declaration of the scheduler we will be using:

```C++
#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aiodatagram.hpp"
#include "solid/frame/aio/aiosocket.hpp"

#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include <signal.h>
#include <iostream>
#include <functional>

using namespace std;
using namespace solid;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
```

Next we need a function to convert the program arguments to a data structure that we will be using later:

```C++
bool parseArguments(Params &_par, int argc, char *argv[]){
    _par.listener_port = 0;
    _par.talker_port = 0;
    if(argc > 1){
        _par.listener_port = atoi(argv[1]);
    }
    if(argc > 2){
        _par.talker_port = atoi(argv[2]);
    }
    return true;
}
```

As you can see the Params structure has only two fields:
 * the TCP listener port
 * and the UDP talker port.

As you will see further, both the Listener and Talker will listen for data on all interfaces ("0.0.0.0").

Let us start with the "main" function's part in which we parse the program arguments and ignore the SIGPIPE signal:

```C++
int main(int argc, char *argv[]){
    Params p;
    if(not parseArguments(p, argc, argv)) return 0;

    signal(SIGPIPE, SIG_IGN);
```

Next, we will be instantiating the SolidFrame Asynchronous environment:
 * The __scheduler__. The active container for solid::frame::aio::Actors. It provides IO events, timer events and custom events to Actors.
 * The __manager__. A passive container of solid::frame::Services.
 * The __service__. A passive container for solid::frame::ActorBase.

The __manager__ allows notifying different actors with custom events while the __service__ allows broadcasting custom events to all its actors.
So, if you want to notify a single actor with a specific event, you'll use the manager:

```C++
solid::frame::ActorUidT objuid = scheduler.startActor(/*...*/);
//...
manager.notify(objuid, make_event(GenericEventE::Message));
```

While if you want to broadcast a specific event to all actors from a service you will use the service:

```C++
//...
service.notifyAll(generic_event_stop);
```

Now, let us go back to the code, instantiate the above actors and start the scheduler with a single running thread:

```C++
AioSchedulerT       scheduler;


frame::Manager      manager;
frame::ServiceT     service(manager);

if(scheduler.start(1/*a single thread*/)){
    cout<<"Error starting scheduler"<<endl;
    return 0;
}
```

Next we will instantiate and start a Listener (which is a solid::frame::aio::Actor):

```C++
{
    ResolveData     rd = synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
    SocketDevice    sd;

    sd.create(rd.begin());
    sd.prepareAccept(rd.begin(), 2000);

    if(sd.ok()){

        {
            SocketAddress   sa;
            sd.localAddress(sa);
            cout<<"Listening for TCP connections on port: "<<sa<<endl;
        }

        solid::ErrorConditionT              error;
        solid::frame::ActorIdT             objuid;

        objuid = scheduler.startActor(make_dynamic<Listener>(service, scheduler, std::move(sd)), service, make_event(GenericEventE::Start), error);
        (void)objuid;
    }else{
        cout<<"Error creating listener socket"<<endl;
        return 0;
    }
}
```

In the first four lines of the above code we prepare a socket device for listening for new connections. Then, if the socket device is OK we go on and print the local address of the socket then we instantiate a Listener actor. The listener actor will need:
 * a reference to the __service__ - all accepted connections will be registered onto the given service;
 * a reference to the __scheduler__ - all accepted connection will be scheduled onto the given scheduler;
 * the socket device previously prepared.

After the Listener actor is created it must be _atomically_:
 * Registered onto service.
 * Scheduled onto scheduler.

This is done in the line:

```C++
objuid = scheduler.startActor(make_dynamic<Listener>(service, scheduler, std::move(sd)), service, make_event(GenericEventE::Start), error);
```

The startActor method parameters are:
 * _objptr_: a smart pointer to a solid::frame::aio::Actor - in our case the listener;
 * _service_: reference to the service which will keep the actor;
 * _event_: the first event to be delivered to the actor if it gets scheduled onto scheduler.

As you will soon see in the declaration of Listener class, every solid::frame::aio::Actor must override the onEvent method to handle the notification events sent to the actor:

```C++
void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
```

Now, lets get back to the main function and instantiate a Talker (a UDP socket) with a code block similar to that for Listener:

```C++
{
    ResolveData     rd = synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
    SocketDevice    sd;

    sd.create(rd.begin());
    sd.bind(rd.begin());

    if(sd.ok()){

        {
            SocketAddress   sa;
            sd.localAddress(sa);
            cout<<"Listening for UDP datagrams on port: "<<sa<<endl;
        }

        solid::ErrorConditionT              error;
        solid::frame::ActorIdT             objuid;

        objuid = scheduler.startActor(make_dynamic<Taker>(std::move(sd)), service, make_event(GenericEventE::Start), error);

        (void)objuid;

    }else{
        cout<<"Error creating talker socket"<<endl;
        return 0;
    }
}
```

We'll get to the declarations for Listener and Talker below but for now lets finish with the main function by waiting for user input to terminate the application:

```C++
    cout<<"Press ENTER to terminate..."<<endl;
    cin.ignore();
    return 0;
}
```

Now, before delving into the Listener and Talker code, lets see what happens after the user of the application enters a character and presses ENTER.

The SolidFrame Asynchronous environment shuts down in the following order:
 * the _service_
   * stops accepting new actors
   * notifies all existing actors with make_event(GenericEventE::Kill)
   * waits until all actors die
 * the _manager_ (because every _service_ has a reference to the _manager_, the _manager_ must outlive all services)
   * ensures that all services are stopped
 * the _scheduler_ (because every _manager_ keep internally references to _shedulers_, _schedulers_ used by _actors_ stored within a _manager_ MUST outlive the _manager_
   * waits until all its threads terminate

Let us now see the declaration of the Listener:

```C++
class Listener: public frame::aio::Actor{
public:
    Listener(
        frame::Service &_rsvc,
        AioSchedulerT &_rsched,
        SocketDevice &&_rsd
    ):
        rservice(_rsvc), rscheduler(_rsched), sock(this->proxy(), std::move(_rsd)), timer(this->proxy()), timercnt(0)
    {}
private:
    void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
    void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);

    using ListenerSocketT = frame::aio::Listener;
    using TimerT = frame::aio::Timer;

    frame::Service      &rservice;
    AioSchedulerT       &rscheduler;
    ListenerSocketT     sock;
    TimerT              timer;
};
```

The definition of the __onEvent__ method is simple - only handle Start and Kill events:

```C++
/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
    if(generic_event<GenericEventE::Start> == _revent){
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
    }else if(generic_event<GenericEventE::Kill> == _revent){
        postStop(_rctx);
    }
}
```

For the Start event we're launching a _fully asynchronous_ __accept__ operation which will complete on __onAccept__ method through the given lambda.
For the Kill event we only schedule actor stop.

__Notes__
 1. Some of the asynchronous IO operations supported by solid::frame::aio library have two forms:
   * a _fully asynchronous_ one (those with the "post" prefix) for which the result is only given by the mean of the given function callback
   * an _asynchronous_ one: the result may either be returned immediately after the IO function call exits or if the operation cannot be completed right away through the given callback function.
 2. The postStop method call atomically:
   * prevents any new notification event be posted for the actor
   * schedules the actor de-registration from the scheduler and from its service after all currently undelivered/pending notification events are delivered to the actor.

Let us further see the definition of the __onAccept__ method:

```C++
void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
    unsigned    repeatcnt = 4;

    do{
        if(!_rctx.error()){
            solid::ErrorConditionT              err;

            rscheduler.startActor(make_dynamic<Connection>(std::move(_rsd)), rservice, make_event(GenericEventE::Start), err);
        }else{
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            timer.waitFor(
                _rctx,
                 NanoTime(10),
                [this](frame::aio::ReactorContext &_rctx){
                    sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
                }
            );
            break;
        }
        --repeatcnt;
    }while(repeatcnt && sock.accept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);}, _rsd));

    if(!repeatcnt){
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
        );//fully asynchronous call
    }
}

```

The above function tries to accept at most 4 connections that are immediately available. If more than 4 connections are available for accept we call the fully asynchronous version of accept (we allow this way for the Reactor to process other actors events too).

Interesting, is that for an error we do not stop the Listener actor but use a timer to wait for 10 seconds before we retry accepting new connections. This is because most certain, the reason why we cannot accept new connections is that a system limit on file descriptors has beer reached and we must wait until some descriptors get released.

In the above code, we've introduced a new frame::aio::Actor - the Connection. Let us see its declaration:

```C++
class Connection: public frame::aio::Actor{
public:
    Connection(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)){}
private:
    void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
    static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext &_rctx);
private:
    using  StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
    enum {BufferCapacity = 1024 * 2};

    char            buf[BufferCapacity];
    StreamSocketT   sock;
};
```

The __Connection::onEvent__ implementation is somehow similar to the one from the Listener:

```C++
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
    if(generic_event<GenericEventE::Start> == _revent){
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
    }else if(generic_event<GenericEventE::Kill> == _revent){
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}
```

Next is the code for __Connection::onRecv__ and for __Connection::onSend__ which is pretty straight forward:


```C++
/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
    unsigned    repeatcnt = 4;
    Connection  &rthis = static_cast<Connection&>(_rctx.actor());
    do{
        if(!_rctx.error()){
            if(rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)){
                if(_rctx.error()){
                    rthis.postStop(_rctx);
                    break;
                }
            }else{
                break;
            }
        }else{
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    }while(repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));

    if(repeatcnt == 0){
        bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
        solid_assert(!rv);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
    Connection &rthis = static_cast<Connection&>(_rctx.actor());
    if(!_rctx.error()){
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
    }else{
        rthis.postStop(_rctx);
    }
}
```

The interesting part on the above code is that we're using static completion callbacks - __onRecv__ and __onSend__ - which theoretically are faster with the downside that we're loosing type information and we have to use a not so nice static_cast.

The Connection::onRecv method uses a loop similar to the one from __Listener::onAccept__: we try to consume as much data already available on the socket as possible (the internal read buffer for socket is 16KB/32KB or more and we do the reading with a 2KB buffer).

Moving on to the Talker (a frame::aio::Actor).

First with its declaration:

```C++
class Talker: public frame::aio::Actor{
public:
    Talker(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)){}
private:
    void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
    void onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz);
    void onSend(frame::aio::ReactorContext &_rctx);
private:
    using DatagramSocketT = frame::aio::Datagram<frame::aio::Socket>;

    enum {BufferCapacity = 1024 * 2 };

    char            buf[BufferCapacity];
    DatagramSocketT sock;
};
```

Secondly with its __onEvent__ method:

```C++
/*virtual*/ void Talker::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
    if(generic_event<GenericEventE::Start> == _revent){
        sock.postRecvFrom(
            _rctx, buf, BufferCapacity,
            [this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
        );//fully asynchronous call
    }else if(generic_event<GenericEventE::Kill> == _revent){
        postStop(_rctx);
    }
}
```

Lastly with its __onRecv__ and __onSend__ completion callbacks:

```C++
void Talker::onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){
    unsigned    repeatcnt = 4;
    do{
        if(!_rctx.error()){
            if(sock.sendTo(_rctx, buf, _sz, _raddr, [this](frame::aio::ReactorContext &_rctx){onSend(_rctx);})){
                if(_rctx.error()){
                    postStop(_rctx);
                    break;
                }
            }else{
                break;
            }
        }else{
            postStop(_rctx);
            break;
        }
        --repeatcnt;
    }while(
        repeatcnt and
        sock.recvFrom(
            _rctx, buf, BufferCapacity,
            [this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}, _raddr, _sz
        )
    );

    if(repeatcnt == 0){
        sock.postRecvFrom(
            _rctx, buf, BufferCapacity,
            [this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
        );//fully asynchronous call
    }
}

void Talker::onSend(frame::aio::ReactorContext &_rctx){
    if(!_rctx.error()){
        sock.postRecvFrom(
            _rctx, buf, BufferCapacity,
            [this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
        );//fully asynchronous call
    }else{
        postStop(_rctx);
    }
}
```

In the above code we've moved back to using lambdas for completion. Although the code seems a little bit more verbose than the static callbacks variant, it allows further simplifications such as directly putting the code from __onSend__ within its calling lambda making the onSend method unnecessary.


## Conclusion

In this tutorial you have learned about basic usage of the solid_frame and solid_frame_aio libraries. You have learned:
 * How to create a Listener Actor.
 * How to accept new connections
 * How to read from connection and write it back to it
 * How to create a Talker for UDP communication
 * How to receive UDP data and how to send it back
 * How to create and start aio::Actors
 * How to notify aio::Actors

## Next

If you are interested on a higher level communication engine you can check out the echo tutorial which uses solid_frame_mprpc (Message Passing - Remote Procedure Call) library:
 * [MPRPC Echo](../mprpc_echo)
