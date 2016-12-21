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
 * __follow the mpipc request tutorial__: [mpipc_request](../mpipc_request)
 
## Overview

In this tutorial we will extend the previous client-server applications by adding:
 * polymorphic keys to the request message
 * support for encrypting the communication via [OpenSSL](https://www.openssl.org/)
 * support for compressing the communication via [Snappy](https://google.github.io/snappy/)

We will further delve into the differences from the previous tutorial.
 
## Protocol definition

As you recall, the Request from the previous tutorial only contained a string "userid_regex" used for filtering the server records.
Now we will use a more powerful command message with support for polymorphic keys. Here is its declarations from mpipc_request_messages.hpp:

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

Lets further dissect the RequestKey:
 * print: virtual method for printing the contents of the key to a std::ostream
 * visit: two virtual methods used for visiting the key with either a visitor which can modify the content of the key or one which cannot.
 * cache_idx: a member variable which does not get serialized and is used for some optimizations on the server side as we will see below.
 
Lets have a look at the generic visitors:

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

Both visitors define virtual visit methods for each and every RequestKey types.
Also both visitors will be used on the server side to browse the Keys from a Request message.

Before declaring the RequestKeys themselves, we need a helper template struct for key visiting:

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

We will continue by presenting only the differences from the previous tutorial regarding the client side code.

ipc_request_client::MessageSetup must be changed as follows:

```C++
template <typename T>
struct MessageSetup;

template <>
struct MessageSetup<RequestKeyAnd>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAnd>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyOr>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOr>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyAndList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAndList>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyOrList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOrList>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyUserIdRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyUserIdRegex>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyEmailRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyEmailRegex>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyYearLess>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyYearLess>(_protocol_idx);
	}
};


template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};

```

The above code will register onto the serialization engine both the message types and the key types.

Next let configure the mpipc::Service with OpenSSL support:

```C++
			frame::mpipc::openssl::setup_client(
				cfg,
				[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
					_rctx.addVerifyAuthority(loadFile("echo-ca-cert.pem"));
					_rctx.loadCertificate(loadFile("echo-client-cert.pem"));
					_rctx.loadPrivateKey(loadFile("echo-client-key.pem"));
					return ErrorCodeT();
				},
				frame::mpipc::openssl::NameCheckSecureStart{"echo-server"}
			);
```

Note that the _pem_ self-signed certificates files above must be on the same directory from where the application is run, otherwise an absolute path would be more appropriate.

To add Snappy communication compress we just need the following line:

```C++
	frame::mpipc::snappy::setup(cfg);
```

Both code snippets above must be added just before:

```C++
	err = ipcservice.reconfigure(std::move(cfg));
```

The last code snippets for client side, constructs a somehow hard-coded Request as follows:

```C++
					auto req_ptr = make_shared<ipc_request::Request>(
						make_shared<ipc_request::RequestKeyAndList>(
							make_shared<ipc_request::RequestKeyOr>(
								make_shared<ipc_request::RequestKeyUserIdRegex>(line.substr(offset + 1)),
								make_shared<ipc_request::RequestKeyEmailRegex>(line.substr(offset + 1))
							),
							make_shared<ipc_request::RequestKeyOr>(
								make_shared<ipc_request::RequestKeyYearLess>(2000),
								make_shared<ipc_request::RequestKeyYearLess>(2003)
							)
						)
					);
					
```

and prints the its key tree to standard output:

```C++
					cout<<"Request key: ";
					if(req_ptr->key) req_ptr->key->print(cout);
					cout<<endl;
```

before sending the message command to the server.


### Compile

```bash
$ cd solid_frame_tutorials/mpipc_request
$ c++ -o mpipc_request_client mpipc_request_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame_aio_openssl -lsolid_frame -lsolid_utility -lsolid_system -lssl -lcrypto -lsnappy -lpthread
```

## The server implementation

In this section we'll have a look at the differences on the server side application code.
So the differences from the server implementation on the previous tutorial are related to:
 * Request command
   * the way that the serialization engine must be configured
   * the way the command is handled - using RequestKeyVisitors
 * mpipc::Service configuration
   * with support for OpenSSL
   * with support for communication compression via Snappy

We'll start by configuring the serialization engine for the new Request.
For that, we continue to have the same line for configuring the protocol:

```C++
ipc_request::ProtoSpecT::setup<ipc_request_server::MessageSetup>(*proto);
```

but now, ipc_request_server::MessageSetup is a little more complex:

```C++
template <typename T>
struct MessageSetup;

template <>
struct MessageSetup<RequestKeyAnd>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAnd>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyAnd, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyOr>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOr>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyOr, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyAndList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAndList>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyAndList, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyOrList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOrList>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyOrList, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyUserIdRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyUserIdRegex>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyUserIdRegex, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyEmailRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyEmailRegex>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyEmailRegex, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyYearLess>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyYearLess>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyYearLess, RequestKey>();
	}
};

template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};
```

So, in the above code we specialize MessageSetup for every type in the ipc_request::ProtoSpecT, but:
 * while command messages are registered as before with a complete_message callback
 * the RequestKeys are registered using the basic version of registerType (without callback) and a call to registerCast from the concrete RequestKey class to the base Key class namely RequestKey.

 The above registerCast is needed because the Request command and the RequestKeys only hold shared_ptrs to the base class (RequestKey).
 
 The complete_message<Response> is same as in previous tutorial, but complete_message<Request> function has changed like this:
 
 ```C++
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
	
	cout<<"Received request: ";
	if(_rrecv_msg_ptr->key){
		_rrecv_msg_ptr->key->print(cout);
	}
	cout<<endl;
	
	
	auto msgptr = std::make_shared<ipc_request::Response>(*_rrecv_msg_ptr);
	
	if(_rrecv_msg_ptr->key){
		PrepareKeyVisitor	prep;
		
		_rrecv_msg_ptr->key->visit(prep);
	
		for(const auto &ad: account_dq){
			AccountDataKeyVisitor v(ad, prep);
			
			
			_rrecv_msg_ptr->key->visit(v);
			
			if(v.retval){
				msgptr->user_data_map.insert(std::move(ipc_request::Response::UserDataMapT::value_type(ad.userid, make_user_data(ad))));
			}
		}
}
	
	SOLID_CHECK_ERROR(_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}
 ```

In the above code we're using two RequestKey visitors:
 * A non-const visitor - PrepareKeyVisitor - which builds a cache of std::regex-es for every key that needs regex matches and store the cache id in the RequestKey::cache_idx.
 * A const visitor - AccountDataKeyVisitor - which is used for deciding whether a database record should be sent to the client or skipped.
 
 Here is PrepareKeyVisitor implementation:
 
 ```C++
using namespace ipc_request;

struct PrepareKeyVisitor: RequestKeyVisitor{
	std::vector<std::regex>		regexvec;
	
	void visit(RequestKeyAnd& _k) override{
		if(_k.first){_k.first->visit(*this);}
		if(_k.second){_k.second->visit(*this);}
	}
	
	void visit(RequestKeyOr& _k) override{
		if(_k.first){_k.first->visit(*this);}
		if(_k.second){_k.second->visit(*this);}
	}
	
	void visit(RequestKeyAndList& _k) override{
		for(auto &k: _k.key_vec){
			if(k) k->visit(*this);
		}
	}
	
	void visit(RequestKeyOrList& _k) override{
		for(auto &k: _k.key_vec){
			if(k) k->visit(*this);
		}
	}
	
	void visit(RequestKeyUserIdRegex& _k) override{
		_k.cache_idx = regexvec.size();
		regexvec.emplace_back(_k.regex);
	}
	
	void visit(RequestKeyEmailRegex& _k) override{
		_k.cache_idx = regexvec.size();
		regexvec.emplace_back(_k.regex);
	}
	
	void visit(RequestKeyYearLess& /*_k*/) override{
		
	}
};
```

So, this visitor does:
 * registers/caches the regex onto a vector and holds the cache position on the key's cache_idx member variable (for RequestKeyUserIdRegex and RequestKeyEmailRegex keys)
 * forward the visit to all child Keys (RequestKeyAnd, RequestKeyOr, RequestKeyAndList and RequestKeyOrList)
 * nothing for RequestKeyYearLess
 
Lets have a look at how AccountDataKeyVisitor is implemented:

```C++
struct AccountDataKeyVisitor: RequestKeyConstVisitor{
	const AccountData	&racc;
	PrepareKeyVisitor	&rprep;
	bool				retval;
	
	AccountDataKeyVisitor(const AccountData &_racc, PrepareKeyVisitor &_rprep):racc(_racc), rprep(_rprep), retval(false){}
	
	void visit(const RequestKeyAnd& _k) override{
		retval = false;
		
		if(_k.first){
			_k.first->visit(*this);
			if(!retval) return;
		}else{
			retval = false;
			return;
		}
		if(_k.second){
			_k.second->visit(*this);
			if(!retval) return;
		}
	}
	
	void visit(const RequestKeyOr& _k) override{
		retval = false;
		if(_k.first){
			_k.first->visit(*this);
			if(retval) return;
		}
		if(_k.second){
			_k.second->visit(*this);
			if(retval) return;
		}
	}
	
	void visit(const RequestKeyAndList& _k) override{
		retval = false;
		for(auto &k: _k.key_vec){
			if(k){
				k->visit(*this);
				if(!retval) return;
			}
		}
	}
	void visit(const RequestKeyOrList& _k) override{
		for(auto &k: _k.key_vec){
			if(k){
				k->visit(*this);
				if(retval) return;
			}
		}
		retval = false;
	}
	
	void visit(const RequestKeyUserIdRegex& _k) override{
		retval = std::regex_match(racc.userid, rprep.regexvec[_k.cache_idx]);
	}
	void visit(const RequestKeyEmailRegex& _k) override{
		retval = std::regex_match(racc.email, rprep.regexvec[_k.cache_idx]);
	}
	
	void visit(const RequestKeyYearLess& _k) override{
		retval = racc.birth_date.year < _k.year;
	}
};
```

So, this time we have a const visitor - we do not need to make changes on the keys.
The AccountDataKeyVisitor has:
 * a const reference to the current database record which must be validated
 * a reference to the PrepareKeyVisitor for its regex cache
 * a retval bool variable.
 
I will not delve into the details for every visit function as I believe they are pretty straight forward, but I would like to point out that:
 * for "OR" like RequestKeys we only visit child keys until a key accepts the database record
 * for "AND" like RequestKeys we only visit child keys until a key rejects the database record

 
Moving on with the changes on server code next is the code that enables SSL support:

```C++
			frame::mpipc::openssl::setup_server(
				cfg,
				[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
					_rctx.loadVerifyFile("echo-ca-cert.pem");
					_rctx.loadCertificateFile("echo-server-cert.pem");
					_rctx.loadPrivateKeyFile("echo-server-key.pem");
					return ErrorCodeT();
				},
				frame::mpipc::openssl::NameCheckSecureStart{"echo-client"}//does nothing - OpenSSL does not check for hostname on SSL_accept
			);
```

along with the code that enables Snappy communication compression:

```C++
	frame::mpipc::snappy::setup(cfg);
```

Both the above snippets of code should be put just above the following line:

```C++
	err = ipcservice.reconfigure(std::move(cfg));
```

### Compile

```bash
$ cd solid_frame_tutorials/mpipc_request
$ c++ -o mpipc_request_server mpipc_request_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame_aio_openssl -lsolid_frame -lsolid_utility -lsolid_system -lssl -lcrypto -lsnappy -lpthread
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

If you are still interested what solid_frame_mpipc library has to offer, check-out the next tutorial 
 
 * [MPIPC File](../mpipc_file)
 
in which you will learn how to implement a very basic remote file access protocol.
