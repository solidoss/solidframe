# solid::frame::mpipc file transfer tutorial

Exemplifies the use of solid_frame_mpipc, solid_frame_aio and solid_frame libraries

__Source files__
 * Message definitions: [mpipc_file_messages.hpp](mpipc_file_messages.hpp)
 * The server: [mpipc_file_server.cpp](mpipc_file_server.cpp)
 * The client: [mpipc_file_client.cpp](mpipc_file_client.cpp)

Before continuing with this tutorial, you should:
 * prepare a SolidFrame build as explained [here](../../README.md#installation).
 * read the [overview of the asynchronous active object model](../../solid/frame/README.md).
 * read the [informations about solid_frame_mpipc](../../solid/frame/mpipc/README.md)
 * follow the first ipc tutorial: [mpipc_echo](../mpipc_echo)

## Overview

In this tutorial you will learn how to use solid_frame_mpipc library for a simple remote file access client-server application pair.
Using the pair of applications we're going to implement, you will be able to list the remote file-system nodes and copy files from remote host to localhost.


**The server**:
 * allows clients to fetch the list of filesystem nodes under a given path using boost_filesystem library;
 * allows clients to fetch files on the server given their remote path.

**The client**:
 * for every command line input
 * extract the recipient endpoint
 * extract command: L for listing C for file copy
 * for L command:
   * extract the remote path
   * creates a ListRequest message and sends it to recipient endpoint
 * for C command:
   * extract the remote path and the local path
   * creates a FileRequest message and send it to recipient endpoint

Notable for the client is the fact that for sending the requests, we're using a variant of mpipc::Service::sendRequest with a Lambda parameter as the completion callback.

Remember that the message completion callback is called when:
 * A message failed to be sent.
 * A message that is not waiting for a response, was sent.
 * A response was received (for a message waiting for it).

You will need three source files:
 * _mpipc_file_messages.hpp_: the protocol messages.
 * _mpipc_file_client.cpp_: the client implementation.
 * _mpipc_file_server.cpp_: the server implementation.

## Protocol definition

The protocol will consist of two request and two response messages.
First enter the list messages:

```C++
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include <vector>
#include <deque>
#include <fstream>
#include <iostream>

namespace ipc_file{

struct ListRequest: solid::frame::mpipc::Message{
    std::string         path;

    ListRequest(){}

    ListRequest(std::string && _path): path(std::move(_path)){}

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.path, _rctx, "path");
    }
};

struct ListResponse: solid::frame::mpipc::Message{
    std::deque<std::pair<std::string, uint8_t> >    node_dq;

    ListResponse(){}

    ListResponse(const ListRequest &_rmsg):solid::frame::mpipc::Message(_rmsg){}

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.node_dq, _rctx, "nodes");
    }
};
```

The ListRequest message consists only of a single string - the remote path for which we want the child nodes.
The ListResponse message contains a std::deque of file-system node names along with their types (file/directory).

Next is the FileRequest message:

```C++
struct FileRequest: solid::frame::mpipc::Message{
    std::string         remote_path;
    std::string         local_path;


    FileRequest(){}

    FileRequest(
        std::string && _remote_path, std::string && _local_path
    ): remote_path(std::move(_remote_path)), local_path(std::move(_local_path)){}

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.remote_path, _rctx, "remote_path");
    }
};
```
which consists of two strings - the remote path and the local path. The interesting thing is that only the _remote_path_ string gets serialized, the local_path is set only to be used by the FileResponse on deserialization to open/create the local file.

Last is the FileResponse message with its little more complicated serialization method:

```C++
struct FileResponse : solid::frame::mpipc::Message {
    std::string remote_path;
    int64_t     remote_file_size;

    FileResponse() {}

    FileResponse(
        const FileRequest& _rmsg)
        : solid::frame::mpipc::Message(_rmsg)
        , remote_path(_rmsg.remote_path)
        , remote_file_size(solid::InvalidSize())
    {
    }

    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) const
    {
        std::ifstream ifs;
        ifs.open(remote_path);
        if (ifs) {
            std::streampos pos = ifs.tellg();
            ifs.seekg(0, ifs.end);
            std::streampos endpos = ifs.tellg();
            ifs.seekg(pos);
            const int64_t file_size = endpos;
            _s.add(file_size, _rctx, "remote_file_size");

            _s.push([ifs = std::move(ifs)](S & _s, solid::frame::mpipc::ConnectionContext & _rctx, const char* _name) mutable {
                _s.add(ifs, [](std::istream& _ris, uint64_t _len, const bool _done, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {
                    //idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                },
                    _rctx, _name);
                return true;
            },
                _rctx, _name);
        } else {
            const int64_t file_size = solid::InvalidSize();
            _s.add(file_size, _rctx, "remote_file_size");
        }
    }
    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        _s.add(remote_file_size, _rctx, "remote_file_size");
        _s.add([this](S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {
            if (remote_file_size != solid::InvalidIndex()) {
                std::ofstream      ofs;
                const std::string* plocal_path = localPath(_rctx);
                if (plocal_path != nullptr) {
                    ofs.open(*plocal_path);
                }
                _s.push([ this, ofs = std::move(ofs) ](S & _s, solid::frame::mpipc::ConnectionContext & _rctx, const char* _name) mutable {
                    _s.add(ofs, [this](std::ostream& _ros, uint64_t _len, const bool _done, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) {

                        //idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                    },
                        _rctx, _name);
                    return true;
                },
                    _rctx, _name);
            }
        },
            _rctx, _name);
    }

private:
    const std::string* localPath(solid::frame::mpipc::ConnectionContext& _rctx) const
    {
        auto req_ptr = std::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if (req_ptr) {
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};
```

Lets delve a little into the _solidSerializeV2_ methods.

**First, some background**

The serialization engine used by the mpipc protocol, is asynchronus, which means that serialization/deserialization of an item might not happen inplace but at a later mement when more buffer space is available (in case of serialization) or more data is available (in case of deserialization). In this situations the serialization item is pushed in a completion queue to be handled later.

FileResponse has two solidSerializeV2 methods:
 * a const one used by serilizers
 * a non-const one used by deserializer.

The const serialization method, tries to open a file stream for reading given a path (this would happen on the server side, when sending the response back to the client).
 * On succes, it determines the file size and adds that value as serialization item. Then pushes a lambda which when executed, it adds the file stream as serialization item (the lambda given to add(ofs, ...) is used for progress monitoring).
 * On failure it just adds an InvalidSize (-1) as serialization item.

Serilizers and deserializers support adding a closure (a function or function object or lambda) as serialization item. The closure is called inplace if the completion queue is empty otherwise it is scheduled on the queue. __Pushing__ a closure onto serializer/deserializer forces it into the completion queue. The items added by the closure are pushed into the completion queue in front of the item containing the closure, and the closure is destroyed only after the items added by it got completed. This is usefull in our case because it will keep the file stream alive until after its serialization has finished.

Please note that the lambda that we're pushing onto the serializer is mutable. This is because we want to be able to modify the captured stream from within the lambda.

Lets move on to the deserialization method where we first add "remote_file_size" item for deserialization. Next we should check it to see if we should open an output stream or not. Because of the asynchronous nature of the deserializer, we cannot do the check directly because "remote_file_size" might be scheduled for deserialization instead of being deserilized inplace. So, we do the check inside a closure added to the deserializer which will be called after the remote_file_size item got deserialized.

Inside the lambda, we decide whether we should open an output stream or not, based on remote_file_size value. Once we've decided that a stream is to be deserialized, we try to open an output stream and continue with stream deserialization not mattering if the stream was successfully opened or not. The same as on the serialization side, we're __pushing__ a closure containing the stream onto the deserializer in order to ensure that the stream object outlives its deserialization process.

---

__Note on move only lambdas and std::function__

In the above code, we're using mutable lambdas capturing a move only object (the stream), using a C++14 addition - capture initializers.

When implementing the new version of serializer and deserilizer, std::function was used for storing callable objects for deferred execution. The problem is that at least on g++ (last version tested: 7.3.1) implementation of std::function, the Callable object must be copy constructible, even though it is "moved" onto the std::function.


This is why [solid::Function](../../solid/utility/function.hpp) was created and got used across SolidFrame project.

---

FileResponse and FileRequest are also examples of how and when to access the Request Message that is waiting for the response from within the response's serialization method. This is an effective way to store data that is needed by the response during the deserialization. Another way would have been to use Connection's "any" data (_rctx.any()) but it would have not been such a clean solution.

Another thing worth mentioning about the above messages is that every response message has a constructor needing a reference to the request message. This is because the mpipc library keeps some data onto the base mpipc::Message which must be passed from the request to the response (it needs the data to identify the request message waiting the currently received message).

One last thing is needed to finish with the protocol definition - define the ProtoSpec type:

```C++
using ProtocolT = solid::frame::mpipc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<ListRequest>(), 1);
    _r(_rproto, solid::TypeToType<ListResponse>(), 2);
    _r(_rproto, solid::TypeToType<FileRequest>(), 3);
    _r(_rproto, solid::TypeToType<FileResponse>(), 4);
}

```

## The client implementation

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
    auto                        proto = ProtocolT::create();
    frame::mpipc::Configuration cfg(scheduler, proto);

    ipc_file::protocol_setup(ipc_file_client::MessageSetup(), *proto);

    cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());

    cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

    err = ipcservice.reconfigure(std::move(cfg));

    if(err){
        cout<<"Error starting ipcservice: "<<err.message()<<endl;
        return 1;
    }
}
```

The ipc_file_client::MessageSetup is defined like this:

```C++
namespace ipc_file_client{

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    SOLID_CHECK(false); //this method should not be called
}

struct MessageSetup {
    template <class T>
    void operator()(ipc_file::ProtocolT& _rprotocol, TypeToType<T> _t2t, const ipc_file::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
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

            std::istringstream iss(line.substr(offset + 1));

            char            choice;

            iss>>choice;

            switch(choice){
                case 'l':
                case 'L':{

                    std::string     path;
                    iss>>path;

                    ipcservice.sendRequest(
                        recipient.c_str(), make_shared<ipc_file::ListRequest>(std::move(path)),
                        [](
                            frame::mpipc::ConnectionContext &_rctx,
                            std::shared_ptr<ipc_file::ListRequest> &_rsent_msg_ptr,
                            std::shared_ptr<ipc_file::ListResponse> &_rrecv_msg_ptr,
                            ErrorConditionT const &_rerror
                        ){
                            if(_rerror){
                                cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
                                return;
                            }

                            SOLID_CHECK(not _rerror and _rsent_msg_ptr and _rrecv_msg_ptr);

                            cout<<"File List from "<<_rctx.recipientName()<<":"<<_rsent_msg_ptr->path<<" with "<<_rrecv_msg_ptr->node_dq.size()<<" items: {"<<endl;

                            for(auto it: _rrecv_msg_ptr->node_dq){
                                cout<<(it.second ? 'D' : 'F')<<": "<<it.first<<endl;
                            }
                            cout<<"}"<<endl;
                        },
                        0
                    );

                    break;
                }

                case 'c':
                case 'C':{
                    std::string     remote_path, local_path;

                    iss>>remote_path>>local_path;

                    ipcservice.sendRequest(
                        recipient.c_str(), make_shared<ipc_file::FileRequest>(std::move(remote_path), std::move(local_path)),
                        [](
                            frame::mpipc::ConnectionContext &_rctx,
                            std::shared_ptr<ipc_file::FileRequest> &_rsent_msg_ptr,
                            std::shared_ptr<ipc_file::FileResponse> &_rrecv_msg_ptr,
                            ErrorConditionT const &_rerror
                        ){
                            if(_rerror){
                                cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
                                return;
                            }

                            SOLID_CHECK(not _rerror and _rsent_msg_ptr and _rrecv_msg_ptr);
                            SOLID_CHECK(_rsent_msg_ptr->local_path == _rrecv_msg_ptr->local_path);
                            SOLID_CHECK(_rsent_msg_ptr->remote_path == _rrecv_msg_ptr->remote_path);

                            cout<<"Done copy from "<<_rctx.recipientName()<<":"<<_rsent_msg_ptr->remote_path<<" to "<<_rsent_msg_ptr->local_path<<": ";

                            if(_rrecv_msg_ptr->remote_file_size != InvalidSize() and _rrecv_msg_ptr->remote_file_size == stream_size(_rrecv_msg_ptr->fs)){
                                cout<<"Success("<<_rrecv_msg_ptr->remote_file_size<<")"<<endl;
                            }else if(_rrecv_msg_ptr->remote_file_size == InvalidSize()){
                                cout<<"Fail(no remote)"<<endl;
                            }else{
                                cout<<"Fail("<<stream_size(_rrecv_msg_ptr->fs)<<" instead of "<<_rrecv_msg_ptr->remote_file_size<<")"<<endl;
                            }
                        },
                        0
                    );
                    break;
                }
                default:
                    cout<<"Unknown choice"<<endl;
                    break;
            }

        }else{
            cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
        }
    }
}
```

So, in the above loop, for every line we read from the standard input:

 * check if it is the terminal line ('q'/'Q'/"quit") and if so exit the loop;
 * extract the endpoint of the server (address[:port])
 * extract the command option as a single char
 * if the command option is L/l
   * extract the remote path as string
   * create a list request message with the extracted remote path
   * send the message as request with a lambda in which we print the received list of nodes to standard output.
 * it the command option is C/c
   * extract the remote and local paths
   * create a file request message with the above paths
   * send the message as request with a lambda in which we print to stdout whether file transfer succeeded or failed.

### Compile

```bash
$ cd solid_frame_tutorials/mpipc_file
$ c++ -o mpipc_file_client mpipc_file_client.cpp -I~/work/extern/include/ -L~/work/extern/lib -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```
Now that we have a client application, we need a server to connect to. Let's move one on implementing the server.

## The server implementation

We will skip the the initialization of the ipcservice and its prerequisites as it is the same as on the client and we'll start with the ipcservice configuration:

```C++
{
    auto                        proto = ProtocolT::create();
    frame::mpipc::Configuration cfg(scheduler, proto);

    ipc_file::protocol_setup(ipc_file_server::MessageSetup(), *proto);

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
ipc_file::protocol_setup(ipc_file_server::MessageSetup(), *proto);
```

which uses ipc_file_server::MessageSetup defined as follows:

```C++
namespace ipc_file_server{

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<ipc_file::ListRequest>(
    frame::mpipc::ConnectionContext&        _rctx,
    std::shared_ptr<ipc_file::ListRequest>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);

    auto msgptr = std::make_shared<ipc_file::ListResponse>(*_rrecv_msg_ptr);

    fs::path fs_path(_rrecv_msg_ptr->path.c_str() /*, fs::native*/);

    if (fs::exists(fs_path) and fs::is_directory(fs_path)) {
        fs::directory_iterator it, end;

        try {
            it = fs::directory_iterator(fs_path);
        } catch (const std::exception& ex) {
            it = end;
        }

        while (it != end) {
            msgptr->node_dq.emplace_back(std::string(it->path().c_str()), static_cast<uint8_t>(is_directory(*it)));
            ++it;
        }
    }
    SOLID_CHECK(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::ListResponse>(
    frame::mpipc::ConnectionContext&         _rctx,
    std::shared_ptr<ipc_file::ListResponse>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(not _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr);
}

template <>
void complete_message<ipc_file::FileRequest>(
    frame::mpipc::ConnectionContext&        _rctx,
    std::shared_ptr<ipc_file::FileRequest>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);

    auto msgptr = std::make_shared<ipc_file::FileResponse>(*_rrecv_msg_ptr);

    if (0) {
        boost::system::error_code error;

        msgptr->remote_file_size = fs::file_size(fs::path(_rrecv_msg_ptr->remote_path), error);
    }

    SOLID_CHECK(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::FileResponse>(
    frame::mpipc::ConnectionContext&         _rctx,
    std::shared_ptr<ipc_file::FileResponse>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(not _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr);
}

struct MessageSetup {
    template <class T>
    void operator()(ipc_file::ProtocolT& _rprotocol, TypeToType<T> _t2t, const ipc_file::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

}//namespace
```

In the above code block the callbacks associated to ListResponse and FileResponse do nothing but some checking.
On the other hand, the callbacks associated to the request messages received from the client are full of interesting code.

So, when receiving a ListRequest we do:
 * create a new ListResponse message from the ListRequest.
 * using boost_filesystem API we iterate through the child nodes under the given path and populate the ListResponse message node_dq.
 * then send back the response message on the same mpipc connection the request was received.

For the FileRequest, things are simpler:
 * create a FileResponse message from the request (the remote_path and local_path entries will be copied to response)
 * then send back the response message on the same mpipc connection the request was received.

Finally returning to our server's main function, the last code block is one which keeps the server alive until user input:

```C++
cout<<"Press ENTER to stop: ";
cin.ignore();
```
### Compile

```bash
$ cd solid_frame_tutorials/mpipc_file
$ c++ -o mpipc_file_server mpipc_file_server.cpp -I~/work/extern/include/ -L~/work/extern/lib -lboost_filesystem -lboost_system -lsolid_frame_mpipc -lsolid_frame_aio -lsolid_frame -lsolid_utility -lsolid_system -lpthread
```

## Test
**Console-1**:
```BASH
$ ./mpipc_file_server 0.0.0.0:3333
```

**Console-2**:
```BASH
$ ./mpipc_file_client
localhost:3333 l /home/
localhost:3333 c /home/user/file.txt ./file.txt
```
## Next

Next you can learn how to use the relay support from the solid_frame_mpipc library:

 * [MPIPC Relay Echo](../mpipc_echo_relay)

