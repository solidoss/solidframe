# solid::frame::aio echo tutorial

Exemplifies the use of solid_frame_aio and solid_frame libraries.

__Source files__:
 * [aio_echo_server.cpp](aio_echo_server.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).

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

## Build

The best way to build the tutorial is through a CMake project but as it is outside of this tutorial, we'll be using the plain ol' command line:

```bash
$ cd solid_frame_tutorials/aio_echo
$ c++ -o echo_server aio_echo_server.cpp -I ~/work/extern/include/ -L ~/work/extern/lib  -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## The code

First of all we need a function to convert the program arguments to a structure that we'll use later:

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

As you can see the Params structure only have two fields:
 * the TCP listener port
 * and the UDP talker port.

As you will see further both the Listener and Talker will listen for data on all interfaces ("0.0.0.0").

Let us start with the "main" function's part in which we parse the program arguments and ignore the SIGPIPE signal:

```C++
int main(int argc, char *argv[]){
	Params p;
	if(not parseArguments(p, argc, argv)) return 0;
	
	signal(SIGPIPE, SIG_IGN);
```

Next we will instantiate the SolidFrame Asynchronous environment:
 * The __scheduler__. The active container for solid::frame::aio::Objects. It provides IO events, timer events and custom events to Objects.
 * The __manager__. A passive container of solid::frame::Services.
 * The __service__. A passive container for solid::frame::ObjectBase.

The __manager__ allows notifying different objects with custom events while the __service__ allows broadcasting custom events to all its objects.
So, if you want to notify a single object with a specific event, you'll use the manager:

```C++
solid::frame::ObjectUidT objuid = scheduler.startObject(/*...*/);
//...
manager.notify(objuid, generic_event_category.event(GenericEvents::Message));
```

While, if you want to broadcast a specific event to all objects from a service you will use the service:

```C++
//...
service.notifyAll(generic_event_category.event(GenericEvents::Stop));
```

Now let us go back to the code and instantiate the above objects and start the scheduler with a single thread running:

```C++
	AioSchedulerT		scheduler;
	
	
	frame::Manager		manager;
	frame::ServiceT		service(manager);
	
	if(scheduler.start(1/*a single thread*/)){
		cout<<"Error starting scheduler"<<endl;
		return 0;
	}
```

Next we will instantiate and start a Listener solid::frame::aio::Object:

```C++
	{
		ResolveData		rd =  synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
		SocketDevice	sd;
		
		sd.create(rd.begin());
		sd.prepareAccept(rd.begin(), 2000);
		
		if(sd.ok()){
			
			{
				SocketAddress	sa;
				sd.localAddress(sa);
				cout<<"Listening for TCP connections on port: "<<sa<<endl;
			}
			
			DynamicPointer<frame::aio::Object>	objptr(new Listener(service, scheduler, std::move(sd)));
			solid::ErrorConditionT				error;
			solid::frame::ObjectIdT				objuid;
			
			objuid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
			(void)objuid;
		}else{
			cout<<"Error creating listener socket"<<endl;
			return 0;
		}
	}
```

In the first four lines of the above code we prepare a socket device for listening for new connections. If the prepared socket device is ok we go on and print the local address of the socket then we instantiate a Listener object. The listener object will need:
 * a reference to the __service__ - all accepted connections will be registered onto the given service.
 * a reference to the __scheduler__ - all accepted connection will be scheduled onto the given scheduler
 * the socket device previously prepared.

After the Listener object is created it must be atomically:
 * Registered onto service
 * Scheduled onto scheduler

This is done in the line:

```C++
	objuid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
```

The startObject method parameters are:
 * _objptr_: a smart pointer to a solid::frame::aio::Object - in our case the listener.
 * _service_: reference to the service which will keep the object.
 * _event_: The first event to be delivered to the object if it gets scheduled onto scheduler.
 
As you will soon see in the declaration of Listener class, every solid::frame::aio::Object must override the onEvent method to handle the notification events sent to the object:

```C++
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
```

Lets get back to the main function and instantiate a Talker (a UDP socket) with a code block similar to that for Listener:

```C++
	{
		ResolveData		rd =  synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
		SocketDevice	sd;
		
		sd.create(rd.begin());
		sd.bind(rd.begin());
		
		if(sd.ok()){
			
			{
				SocketAddress	sa;
				sd.localAddress(sa);
				cout<<"Listening for UDP datagrams on port: "<<sa<<endl;
			}
			
			DynamicPointer<frame::aio::Object>	objptr(new Talker(std::move(sd)));
			
			solid::ErrorConditionT				error;
			solid::frame::ObjectIdT				objuid;
			
			objuid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
			
			(void)objuid;
			
		}else{
			cout<<"Error creating talker socket"<<endl;
			return 0;
		}
	}
```

We'll see the declarations for Listener and Talker below but for now lets finish with the main function by waiting for user input to terminate the application 

```C++
	cout<<"Press any key and ENTER to terminate..."<<endl;
	char c;
	cin>>c;
	return 0;
}
```

Before delving into the Listener and Talker code, lets see what happens after the user of the application enters a character and presses ENTER:

The SolidFrame Asynchronous environment shuts down in the following order:
 * the _service_: will stop accepting new objects, notify all existing objects with generic_event_category.event(GenericEvents::Kill) and wait until all objects die.
 * the _manager_: because every _service_ has a reference to the _manager_, the _manager_ must outlive all services. Upon destruction, the _manager_ will ensure all services are stopped.
 * the _scheduler_: because every _manager_ keep internally references to _shedulers_, _schedulers_ used by _objects_ stored within a _manager_ MUST outlive the _manager_. On destruction, the _scheduler_ will wait until all its threads terminate.

Let us now see the declaration of the Listener:

```C++
class Listener: public frame::aio::Object{
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
	
	frame::Service		&rservice;
	AioSchedulerT		&rscheduler;
	ListenerSocketT		sock;
	TimerT				timer;
};
```

The definition of the onEvent method is simple: only handle Start and Kill events:

```C++
/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		postStop(_rctx);
	}
}
```

For the Start event we're launching a _fully asynchronous_ Accept operation which will complete on onAccept method through the given lambda.
For the Kill event we only schedule object stop.

__Notes__
 1. Some of the asynchronous IO operations supported by solid::frame::aio library have two forms:
   * a _fully asynchronous_ one (those with the "post" prefix): the result is only given by the mean of the given function
   * an _asynchronous_ one: the result may either be returned immediately after the IO function call exits or if the operation cannot be completed right away through the given callback function.
 2. The postStop method call atomically:
   * prevent any new notification event posting on the object
   * schedules the object de-registration from the scheduler and from its service after all currently undelivered/pending notification events are delivered to the object.

Let us see now the definition of the onAccept method:

```C++
void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			DynamicPointer<frame::aio::Object>	objptr(new Connection(std::move(_rsd)));
			solid::ErrorConditionT				err;
			
			rscheduler.startObject(objptr, rservice, generic_event_category.event(GenericEvents::Start), err);
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

The above function tries to accept at most 4 connections that are immediately available. If more than 4 connections are available for accept we call the fully asynchronous version of accept (we allow this way for the Reactor to process other objects events too).

Interesting is that on error we do not stop the Listener object but use a timer to wait for 10 seconds before we retry accepting new connections. This is because most certain, the reason why we cannot accept new connections is that a system limit on file descriptors has beer reached and we must wait until some descriptors get released.

In the above code, we've introduced a new frame::aio::Object - the Connection - which has the following declaration:

```C++
class Connection: public frame::aio::Object{
public:
	Connection(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)){}
private:
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
private:
	using  StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
	enum {BufferCapacity = 1024 * 2};
	
	char			buf[BufferCapacity];
	StreamSocketT	sock;
};
```

The Connection::onEvent implementation is somehow similar to the one from the Listener:

```C++
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		sock.shutdown(_rctx);
		postStop(_rctx);
	}
}
```

Next the code for Connection::onRecv and Connection::onSend is pretty straight forward:


```C++
/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 4;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
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
		SOLID_ASSERT(!rv);
	}
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
	}else{
		rthis.postStop(_rctx);
	}
}
```

The interesting part on the above code is that we're using static completion callbacks - onRecv and onSend - which theoretically are faster with the downside that we're loosing type information and we have to use a not so nice static_cast.

The Connection::onRecv method uses a loop similar to the one from Listener::onAccept: we try to consume as much data already available on the socket as possible (the internal read buffer for socket is 16KB/32KB or more and we do the reading with a 2KB buffer).

Move on to the Talker frame::aio::Object;

with its declaration:

```C++
class Talker: public frame::aio::Object{
public:
	Talker(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)){}
private:
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
	void onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz);
	void onSend(frame::aio::ReactorContext &_rctx);
private:
	using DatagramSocketT = frame::aio::Datagram<frame::aio::Socket>;
	
	enum {BufferCapacity = 1024 * 2 };
	
	char			buf[BufferCapacity];
	DatagramSocketT	sock;
};
```

its onEvent method:

```C++
/*virtual*/ void Talker::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postRecvFrom(
			_rctx, buf, BufferCapacity,
			[this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
		);//fully asynchronous call
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		postStop(_rctx);
	}
}
```

and the onRecv and onSend completion callbacks:

```C++
void Talker::onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){
	unsigned	repeatcnt = 4;
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

In the above code we've moved back to using lambdas for completion. Although the code seems a little bit more verbose than the static callbacks variant, it allows further simplifications such as directly putting the code from onSend within its calling lambda making the onSend unnecessary.
