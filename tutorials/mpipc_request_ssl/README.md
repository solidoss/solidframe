# solid::frame::mpipc SSL request tutorial

Exemplifies the use of solid_frame_mpipc, solid_frame_aio, solid_frame_aio_openssl and solid_frame libraries

__Source files__
 * Message definitions: [mpipc_request_messages.hpp](mpipc_request_messages.hpp)
 * The server: [mpipc_request_server.cpp](mpipc_request_server.cpp)
 * The client: [mpipc_request_client.cpp](mpipc_request_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mpipc](../../solid/frame/mpipc/README.md)
 * follow the first mpipc tutorial: [mpipc_echo](../mpipc_echo)
 * follow the mpipc request tutorial: [mpipc_request](../mpipc_request)
 
## Overview

In this tutorial we will extend the previous client-server applications:
 * by adding add polymorphic keys to the request message
 * by adding support for encrypting the communication via [OpenSSL](https://www.openssl.org/)
 * by adding support for compressing the communication via [Snappy](https://google.github.io/snappy/)

We will further delve into the differences from the previous tutorial.
 
## Protocol definition

As you recall, the Request from the previous tutorial only contained a string "userid_regex" used for filtering the server records.
Now we will use a more powerfull command with support for polymorphic keys. Here is its declarations from mpipc_request_messages.hpp:

```C++
struct Request: solid::frame::mpipc::Message{
	std::shared_ptr<RequestKey>	key;
	
	Request(){}
	
	Request(std::shared_ptr<RequestKey> && _key): key(std::move(_key)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(key, "key");
	}	
};
```

So, now the Request will only hold a shared_ptr to a generic RequestKey, declared this way:

```C++
struct RequestKeyVisitor;
struct RequestKeyConstVisitor;

struct RequestKey{
	RequestKey():cache_idx(solid::InvalidIndex{}){}
	virtual ~RequestKey(){}
	
	virtual void print(std::ostream &_ros) const = 0;
	
	virtual void visit(RequestKeyVisitor &) = 0;
	virtual void visit(RequestKeyConstVisitor &) const = 0;

	size_t		cache_idx;//NOT serialized - used by the server to cache certain key data
};
```

Lets further disect the RequestKey:
 * print: virtual method for printing the contents of the key to a std::ostream
 * visit: two virtual methods used for visiting the key with either a visitor which can modify the content of the key or one which cannot.
 * cache_idx: a member variable which does not get serialized and is used for some optimizations on the server side as we will see below.
 
Next lets have a look at the visitors:

```C++
struct RequestKeyAnd;
struct RequestKeyOr;
struct RequestKeyAndList;
struct RequestKeyOrList;
struct RequestKeyUserIdRegex;
struct RequestKeyEmailRegex;
struct RequestKeyYearLess;

struct RequestKeyVisitor{
	virtual ~RequestKeyVisitor(){}
	
	virtual void visit(RequestKeyAnd&) = 0;
	virtual void visit(RequestKeyOr&) = 0;
	virtual void visit(RequestKeyAndList&) = 0;
	virtual void visit(RequestKeyOrList&) = 0;
	virtual void visit(RequestKeyUserIdRegex&) = 0;
	virtual void visit(RequestKeyEmailRegex&) = 0;
	virtual void visit(RequestKeyYearLess&) = 0;
};

struct RequestKeyConstVisitor{
	virtual ~RequestKeyConstVisitor(){}
	
	virtual void visit(const RequestKeyAnd&) = 0;
	virtual void visit(const RequestKeyOr&) = 0;
	virtual void visit(const RequestKeyAndList&) = 0;
	virtual void visit(const RequestKeyOrList&) = 0;
	virtual void visit(const RequestKeyUserIdRegex&) = 0;
	virtual void visit(const RequestKeyEmailRegex&) = 0;
	virtual void visit(const RequestKeyYearLess&) = 0;
};
```

Both visitors define virtual visit methods for each and every RequestKey types. Both visitors will be used on the server side to browse the RequestKeys from a Request command.

Before getting to declaring the RequestKeys themselves, we need a helper struct for key visiting:

```C++
template <class T>
struct Visitable: RequestKey{
	
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*static_cast<T*>(this));
	}
	void visit(RequestKeyConstVisitor &_v) const override{
		_v.visit(*static_cast<const T*>(this));
	}
};
```

Here are all the RequestKey types:

```C++
struct RequestKeyAnd: Visitable<RequestKeyAnd>{
	std::shared_ptr<RequestKey>		first;
	std::shared_ptr<RequestKey>		second;
	
	
	RequestKeyAnd(){}
	
	template <class T1, class T2>
	RequestKeyAnd(
		std::shared_ptr<T1> && _p1,
		std::shared_ptr<T2> &&_p2
	):	first(std::move(std::static_pointer_cast<RequestKey>(_p1))),
		second(std::move(std::static_pointer_cast<RequestKey>(_p2))){}
		
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(second, "second").push(first, "first");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"and{";
		if(first) first->print(_ros);
		_ros<<',';
		if(second) second->print(_ros);
		_ros<<'}';
	}
	
};

struct RequestKeyOr: Visitable<RequestKeyOr>{
	std::shared_ptr<RequestKey>		first;
	std::shared_ptr<RequestKey>		second;
	
	RequestKeyOr(){}
	
	template <class T1, class T2>
	RequestKeyOr(
		std::shared_ptr<T1> && _p1,
		std::shared_ptr<T2> &&_p2
	):	first(std::move(std::static_pointer_cast<RequestKey>(_p1))),
		second(std::move(std::static_pointer_cast<RequestKey>(_p2))){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(second, "second").push(first, "first");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"or(";
		if(first) first->print(_ros);
		_ros<<',';
		if(second) second->print(_ros);
		_ros<<')';
	}
};

struct RequestKeyAndList: Visitable<RequestKeyAndList>{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	RequestKeyAndList(){}
	
	template <class ...Args>
	RequestKeyAndList(std::shared_ptr<Args>&& ..._args):key_vec{std::move(_args)...}{}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"AND{";
		for(auto const &key: key_vec){
			if(key) key->print(_ros);
			_ros<<',';
		}
		_ros<<'}';
	}
};

struct RequestKeyOrList: Visitable<RequestKeyOrList>{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	
	RequestKeyOrList(){}
	
	template <class ...Args>
	RequestKeyOrList(std::shared_ptr<Args>&& ..._args):key_vec{std::move(_args)...}{}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"OR(";
		for(auto const &key: key_vec){
			if(key) key->print(_ros);
			_ros<<',';
		}
		_ros<<')';
	}
};

struct RequestKeyUserIdRegex: Visitable<RequestKeyUserIdRegex>{
	std::string		regex;
	
	RequestKeyUserIdRegex(){}
	
	RequestKeyUserIdRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"userid matches \""<<regex<<"\"";
	}
};

struct RequestKeyEmailRegex: Visitable<RequestKeyEmailRegex>{
	std::string		regex;
	
	RequestKeyEmailRegex(){}
	
	RequestKeyEmailRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"email matches \""<<regex<<"\"";
	}
};


struct RequestKeyYearLess: Visitable<RequestKeyYearLess>{
	uint16_t	year;
	
	RequestKeyYearLess(uint16_t _year = 0xffff):year(_year){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(year, "year");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"year < "<<year;
	}
};
```

Notable on the above declarations are the serialize methods which are just as for every other Message.

Next on the protocol header contains the declarations for the Response message which are the same as in previous tutorial.
The last thing that differs is the protocol definition - which now will contain the RequestKeys too:
```C++
using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<
	0,
	Request,
	Response,
	RequestKeyAnd,
	RequestKeyOr,
	RequestKeyAndList,
	RequestKeyOrList,
	RequestKeyUserIdRegex,
	RequestKeyEmailRegex,
	RequestKeyYearLess
>;
```



## The client implementation

First of all the client we will be implementing will be able to talk to multiple servers. Every server will be identified by its endpoint: address_name:port.
So, the client will read from standard input line by line and:
 * if the line is "q", "Q" or "quit" will exit
 * else
   * extract the first word of the line as the server endpoint
   * extract the reminder of the line as payload (the regular expression) and create a Message with it
   * send the message to the server endpoint

Let us now walk through the code.

First off, initialize the ipcservice and its prerequisites:

```C++
		AioSchedulerT			scheduler;
		
		
		frame::Manager			manager;
		frame::mpipc::ServiceT	ipcservice(manager);
		
		frame::aio::Resolver	resolver;
		
		ErrorConditionT			err;
		
		err = scheduler.start(1);
		
		if(err){
			cout<<"Error starting aio scheduler: "<<err.message()<<endl;
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			cout<<"Error starting aio resolver: "<<err.message()<<endl;
			return 1;
		}
```

Next, configure the ipcservice:
```C++
		{
			auto 						proto = frame::mpipc::serialization_v1::Protocol::create();
			frame::mpipc::Configuration	cfg(scheduler, proto);
			
			ipc_request::ProtoSpecT::setup<ipc_request_client::MessageSetup>(*proto);
			
			cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());
			
			cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				return 1;
			}
		}
```

The ipc_request_client::MessageSetup is defined like this:

```C++
namespace ipc_request_client{

template <class M>
void complete_message(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(false);//this method should not be called
}

template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};


}//namespace
```

Note on the above code that the "catch all" message completion callback should not be called on the client.
It must be specified in the ipcservice configuration, but in our current situation will not get to be used.

And finally we have the command loop:

```C++
		while(true){
			string	line;
			getline(cin, line);
			
			if(line == "q" or line == "Q" or line == "quit"){
				break;
			}
			{
				string		recipient;
				size_t		offset = line.find(' ');
				
				if(offset != string::npos){
					
					recipient = line.substr(0, offset);
					
					auto  lambda = [](
						frame::mpipc::ConnectionContext &_rctx,
						std::shared_ptr<ipc_request::Request> &_rsent_msg_ptr,
						std::shared_ptr<ipc_request::Response> &_rrecv_msg_ptr,
						ErrorConditionT const &_rerror
					){
						if(_rerror){
							cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
							return;
						}
						
						SOLID_CHECK(not _rerror and _rsent_msg_ptr and _rrecv_msg_ptr);
						
						cout<<"Received "<<_rrecv_msg_ptr->user_data_map.size()<<" users:"<<endl;
						
						for(const auto& user_data: _rrecv_msg_ptr->user_data_map){
							cout<<'{'<<user_data.first<<"}: "<<user_data.second<<endl;
						}
					};
					
					ipcservice.sendRequest(
						recipient.c_str(), make_shared<ipc_request::Request>(line.substr(offset + 1)),
						lambda,
						0
					);
				
				}else{
					cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
				}
			}
		}
```
On the above code, the notable part is the one with _ipcservice.sendRequest_ call which uses a lambda to specify the
completion callback for the response. Also note the message types used in the lambda definition - they are the concrete message types
we're expecting.

On the lambda, we display to standard out how many user records that matched the regular expression were returned and then display the records.

### Compile

```bash
$ cd solid_frame_tutorials/mpipc_request
$ c++ -o mpipc_request_client mpipc_request_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The server implementation

We will skip the the initialization of the ipcservice and its prerequisites as it is the same as on the client and we'll start with the ipcservice configuration:

```C++
		{
			auto 						proto = frame::mpipc::serialization_v1::Protocol::create();
			frame::mpipc::Configuration	cfg(scheduler, proto);
			
			ipc_request::ProtoSpecT::setup<ipc_request_server::MessageSetup>(*proto);
			
			cfg.server.listener_address_str = p.listener_addr;
			cfg.server.listener_address_str += ':';
			cfg.server.listener_address_str += p.listener_port;
			
			cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				manager.stop();
				return 1;
			}
			{
				std::ostringstream oss;
				oss<<ipcservice.configuration().server.listenerPort();
				cout<<"server listens on port: "<<oss.str()<<endl;
			}
		}
```

Notable is the protocol implementation:

```C++
	ipc_request::ProtoSpecT::setup<ipc_request_server::MessageSetup>(*proto);
```

which uses ipc_request_server::MessageSetup defined as follows:

```C++
namespace ipc_request_server{

template <class M>
void complete_message(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);
	
template <>
void complete_message<ipc_request::Request>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Request> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Request> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rrecv_msg_ptr);
	SOLID_CHECK(not _rsent_msg_ptr);
	
	auto msgptr = std::make_shared<ipc_request::Response>(*_rrecv_msg_ptr);
	
	
	std::regex	userid_regex(_rrecv_msg_ptr->userid_regex);
	
	for(const auto &ad: account_dq){
		if(std::regex_match(ad.userid, userid_regex)){
			msgptr->user_data_map.insert(std::move(ipc_request::Response::UserDataMapT::value_type(ad.userid, std::move(make_user_data(ad)))));
		}
	}
	
	SOLID_CHECK_ERROR(_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_request::Response>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Response> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Response> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(not _rrecv_msg_ptr);
	SOLID_CHECK(_rsent_msg_ptr);
}

template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};

}//namespace ipc_request_server
```
For the protocol implementation we're using two message completion callbacks - one for request and the other for response.

The callback for response is called on the successful delivery (i.e. successfully sent on socket - NOT necessarily received on client) and it only consist of some checking - no real code.

The request callback, on the other hand, is called when a request is received from a client and does:
 * create a Response message from the Request one
 * filters the accounts table using the regular expression received from the client, populating the response map with matched records.
 * send the Response message back to the client on the same connection the request was received.
 
The accounts table, i.e. the accounts_dq is defined like this:

```C++
struct Date{
	uint8_t		day;
	uint8_t		month;
	uint16_t	year;
};

struct AccountData{
	string		userid;
	string		full_name;
	string		email;
	string		country;
	string		city;
	Date		birth_date;
};


using AccountDataDequeT = std::deque<AccountData>;

const AccountDataDequeT	account_dq = {
	{"user1", "Super Man", "user1@email.com", "US", "Washington", {11, 1, 2001}},
	{"user2", "Spider Man", "user2@email.com", "RO", "Bucharest", {12, 2, 2002}},
	{"user3", "Ant Man", "user3@email.com", "IE", "Dublin", {13, 3, 2003}},
	{"iron_man", "Iron Man", "man.iron@email.com", "UK", "London", {11,4,2004}},
	{"dragon_man", "Dragon Man", "man.dragon@email.com", "FR", "paris", {12,5,2005}},
	{"frog_man", "Frog Man", "man.frog@email.com", "PL", "Warsaw", {13,6,2006}},
};
```
One last thing we need related to accounts table is a conversion function from the data structure we have on the table to the one from the Response message:

```C++
ipc_request::UserData make_user_data(const AccountData &_rad){
	ipc_request::UserData	ud;
	ud.full_name = _rad.full_name;
	ud.email = _rad.email;
	ud.country = _rad.country;
	ud.city = _rad.city;
	ud.birth_date.day = _rad.birth_date.day;
	ud.birth_date.month = _rad.birth_date.month;
	ud.birth_date.year = _rad.birth_date.year;
	return ud;
}
```
Before moving on, lets stop for a moment on a previous statement:
 * create a Response message from the Request one

which translates to the following line of code from the request message completion callback:

```C++
	auto msgptr = std::make_shared<ipc_request::Response>(*_rrecv_msg_ptr);
```
So, a response message MUST be constructed from the request one. This is because some data from the Request message is needed to be passed to the Response. That data will be transparently serialized along with the response when sent back to the client and used on the client to identify the request message waiting for the response.

As an idea, for a message that moves back and forth from client to server, because of mpipc::Message internal data, one can always know on which side a message is, by using the following methods from mpipc::Message:

```C++
	bool isOnSender()const
	bool isOnPeer()const;
	bool isBackOnSender()const;
```

Returning to our server, the last code block is one which keeps the server alive until user input:

```C++
	cout<<"Press any char and ENTER to stop: ";
	char c;
	cin>>c;
```

### Compile

```bash
$ cd solid_frame_tutorials/mpipc_request
$ c++ -o mpipc_request_server mpipc_request_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test

Now that we have two applications a client and a server let us test it in a little scenario with two servers and a client.

**Console-1**:
```BASH
$ ./mpipc_request_server 0.0.0.0:3333
```
**Console-2**:
```BASH
$ ./mpipc_request_client
localhost:3333 [a-z]+_man
127.0.0.1:4444 user\d*
```
**Console-3**:
```BASH
#wait for a while
$ ./mpipc_request_server 0.0.0.0:4444
```

On the client you will see that the records list is immediately received back from :3333 server while the second response is received back only after the second server is started. This is because, normally, the ipcservice will try re-sending the message until the recipient side becomes available. Use **mpipc::MessageFlags::OneShotSend** to change the behavior and only try once to send the message and immediately fail if the server is offline.

## Next

If you are still insterested what solid_frame_mpipc library has to offer, check-out the next tutorial 
 
 * [MPIPC File](../mpipc_file)
 
in which you will learn how to implement a very basic remote file access protocol.
