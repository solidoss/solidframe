# solid::frame::mpipc request tutorial

Exemplifies the use of solid_frame_mpipc, solid_frame_aio and solid_frame libraries

__Source files__
 * Message definitions: [mpipc_request_messages.hpp](mpipc_request_messages.hpp)
 * The server: [mpipc_request_server.cpp](mpipc_request_server.cpp)
 * The client: [mpipc_request_client.cpp](mpipc_request_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mpipc](../../solid/frame/mpipc/README.md)
 * follow the first ipc tutorial: [mpipc_echo](../mpipc_echo)

## Overview

In this tutorial you will learn how to use solid_frame_mpipc library for a simple client-server application pair.
While in previous mpipc tutorial the client and server exchanged a single message for the current tutorial we'll have two, slightly more complex messages to exchage:
 * a request from the client side
 * and a response from the server side

**The server**:
 * keeps a small, static, table (a std::deque of elements)
 * allows clients to fetch records from the table using regular expression.

**The client**:
 * for every command line input
 * extract the recipient endpoint
 * extract payload - the regular expression
 * creates a Request with the regular expression string and sends it to the server at recipient endpoint

Notable for the client is the fact that for sending the request, we're using a variant of mpipc::Service::sendRequest with a Lambda parameter as the completion callback.

Remember that the message completion callback is called when:
 * A message failed to be sent.
 * A message that is not waiting for a response, was sent.
 * A response was received (for a message waiting for it).

You will need three source files:
 * _mpipc_request_messages.hpp_: the protocol messages.
 * _mpipc_request_client.cpp_: the client implementation.
 * _mpipc_request_server.cpp_: the server implementation.

## Protocol definition

As you've seen in the mpipc_echo tutorial, the protocol - i.e. the exchanged messages - is defined in a single header file.
We'll be looking at the header file by splitting it into pieces:

The first piece consists of includes and the definition for the Request message:

```C++
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <vector>
#include <map>

namespace ipc_request{

struct Request: solid::frame::mpipc::Message{
    std::string         userid_regex;

    Request(){}

    Request(std::string && _ustr): userid_regex(std::move(_ustr)){}

    template <class S>
    void solidSerialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(userid_regex, "userid_regex");
    }
};
```

The next piece of code is about the Response message, which, for the sake of exemplifying some of the serialization engine capabilities will make use of two other serializable data structures:

```C++
struct Date{
    uint8_t     day;
    uint8_t     month;
    uint16_t    year;

    template <class S>
    void solidSerialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(day, "day").push(month, "month").push(year, "year");
    }
};

struct UserData{
    std::string     full_name;
    std::string     email;
    std::string     country;
    std::string     city;
    Date            birth_date;

    template <class S>
    void solidSerialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(full_name, "full_name").push(email, "email");
        _s.push(country, "country").push(city, "city").push(birth_date, "birth_date");
    }
};

struct Response: solid::frame::mpipc::Message{
    using UserDataMapT = std::map<std::string, UserData>;

    UserDataMapT    user_data_map;

    Response(){}

    Response(const solid::frame::mpipc::Message &_rmsg):solid::frame::mpipc::Message(_rmsg){}

    template <class S>
    void solidSerialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.pushContainer(user_data_map, "user_data_map");
    }
};
```

On the above code, please note that we're using a std::map for storing the records in the response message which is strictly for exemplification purpose - normally a std::vector would have been a better option.

The last block of code for the protocol definition is the declaration of ProtoSpecT:

```C++
using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, Request, Response>;

}//namespace ipc_request
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
AioSchedulerT           scheduler;


frame::Manager          manager;
frame::mpipc::ServiceT  ipcservice(manager);

frame::aio::Resolver    resolver;

ErrorConditionT         err;

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
    auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
    frame::mpipc::Configuration cfg(scheduler, proto);

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
    string  line;
    getline(cin, line);

    if(line == "q" or line == "Q" or line == "quit"){
        break;
    }
    {
        string      recipient;
        size_t      offset = line.find(' ');

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

            auto req_ptr = make_shared<ipc_request::Request>(line.substr(offset + 1));

            ipcservice.sendRequest(
                recipient.c_str(), req_ptr,
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
    auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
    frame::mpipc::Configuration cfg(scheduler, proto);

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


    std::regex  userid_regex(_rrecv_msg_ptr->userid_regex);

    for(const auto &ad: account_dq){
        if(std::regex_match(ad.userid, userid_regex)){
            msgptr->user_data_map.insert(ipc_request::Response::UserDataMapT::value_type(ad.userid, std::move(make_user_data(ad))));
        }
    }

    SOLID_CHECK(_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
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
    uint8_t     day;
    uint8_t     month;
    uint16_t    year;
};

struct AccountData{
    string      userid;
    string      full_name;
    string      email;
    string      country;
    string      city;
    Date        birth_date;
};


using AccountDataDequeT = std::deque<AccountData>;

const AccountDataDequeT account_dq = {
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
    ipc_request::UserData   ud;
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

In the next tutorial:

 * [MPIPC Request SSL](../mpipc_request_ssl)

we will extend the current example by:
 * adding SSL support for end-to-end encryption
 * adding compression support for communication
 * using a polymorphic request
