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
