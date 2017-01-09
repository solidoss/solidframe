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
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <vector>
#include <deque>
#include <fstream>
#include <iostream>

namespace ipc_file{

struct ListRequest: solid::frame::mpipc::Message{
    std::string         path;
    
    ListRequest(){}
    
    ListRequest(std::string && _path): path(std::move(_path)){}
    
    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(path, "path");
    }   
};

struct ListResponse: solid::frame::mpipc::Message{
    std::deque<std::pair<std::string, uint8_t> >    node_dq;
    
    ListResponse(){}
    
    ListResponse(const ListRequest &_rmsg):solid::frame::mpipc::Message(_rmsg){}
    
    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.pushContainer(node_dq, "node_dq");
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
    
    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.push(remote_path, "remote_path");
    }   
};
```
which consists of two strings - the remote path and the local path. The interesting thing is that only the _remote_path_ string gets serialized, the local_path is set only to be used by the FileResponse on deserialization to open/create the local file.

Last is the FileResponse message with its little more complicated serialization method:

```C++
struct FileResponse: solid::frame::mpipc::Message{
    std::string         remote_path;
    std::fstream        fs;
    int64_t             remote_file_size;
    
    FileResponse(){}
    
    FileResponse(
        const FileRequest &_rmsg
    ):  solid::frame::mpipc::Message(_rmsg), remote_path(_rmsg.remote_path),
        remote_file_size(solid::InvalidSize()){}
    
    template <class S>
    void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
        _s.pushCall(
            [this](S &_rs, solid::frame::mpipc::ConnectionContext &_rctx, uint64_t _val, solid::ErrorConditionT &_rerr){
                if(S::IsSerializer){
                    fs.open(remote_path.c_str());
                    _rs.pushStream(static_cast<std::istream*>(&fs), "fs");
                    
                    if(fs){
                        std::streampos pos = fs.tellg();
                        fs.seekg(0, fs.end);
                        std::streampos endpos = fs.tellg();
                        fs.seekg(pos);
                        remote_file_size = endpos;
                    }else{
                        remote_file_size = solid::InvalidSize();
                    }
                    _rs.push(remote_file_size, "remote_file_size");
                }else{
                    std::string *plocal_path = localPath(_rctx);
                    
                    if(remote_file_size != solid::InvalidSize() and plocal_path){
                        fs.open(plocal_path->c_str(), std::fstream::out | std::fstream::binary);
                    }
                    _rs.pushStream(static_cast<std::ostream*>(&fs), "fs");
                }
            }, 0, "reinit"
        );
        if(not S::IsSerializer){
            _s.push(remote_file_size, "remote_file_size");
        }
    }
private:
    std::string * localPath(solid::frame::mpipc::ConnectionContext &_rctx)const{
        auto req_ptr = std::dynamic_pointer_cast<FileRequest>(_rctx.fetchRequest(*this));
        if(req_ptr){
            return &req_ptr->local_path;
        }
        return nullptr;
    }
};
```

Lets delve a little into the _serialize_ method.

**First, some background**

The serialization engine from solid framework is _lazy_ - this means that the serialization does not happen right-away but it is scheduled for later when input data or output space will be available. That is why, pushing items into serialization engine, schedules them in a stack (LastInFirstOut) like fashion. This is why the elements you push last will actually get serialized first.
In some situation, as is the case with FileResponse, some serialization engine decisions/operations must happen after some certain fields were already serialized. In our case, on the deserialization (client) side, before starting to serialize the stream we need it opened for writing to a file. For that we use the pushCall functionality of the serialization engine - which schedules a call to a lambda after the other message items (in case of deserializer, remote_file_size) were completed.

In the above code, we use remote_file_size as a mean to notify the client side, the deserialization side of the message, that the remote file could not be opened and there is no reason to create the local file (the destination file on the client).

In the lambda, if the engine is a serializer (on the sending side - i.e. the server):
 * open a filestream to the file pointed by remote_path (remember we're on the server)
 * schedules the stream for serialization (pushStream)
 * compute the file size (use an invalid size in case of error)
 * schedules the remote_file_size for serialization (will get serialized before the stream)

Else, if the engine is a Deserializer (on the receiving side - i.e. back on client):
 * If the remote_file_size field we've already parsed has a valid size, open a output stream for a file pointed by localPath (we're on the client and we get the localPath from the request)
 * schedules the stream for deserialization via pushStream. Note that the pushStream is called whether we've opened the file or not letting the serialization engine handle errors.

FileResponse and FileRequest are also examples of how and when to access the Request Message that is waiting for the response from within the response's serialization method. This is an effective way to store data that is needed by the response during the deserialization. Another way would have been to use Connection's "any" data (_rctx.any()) but it would have not been such a clean solution.

Another thing worth mentioning about the above messages is that every response message has a constructor needing a reference to the request message. This is because the mpipc library keeps some data onto the base mpipc::Message which must be passed from the request to the response (it needs the data to identify the request message waiting the currently received message).

One last thing is needed to finish with the protocol definition - define the ProtoSpec type:

```C++
using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, ListRequest, ListResponse, FileRequest, FileResponse>;
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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, proto);
            
            ipc_file::ProtoSpecT::setup<ipc_file_client::MessageSetup>(*proto);
            
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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(scheduler, proto);
            
            ipc_file::ProtoSpecT::setup<ipc_file_server::MessageSetup>(*proto);
            
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
    ipc_file::ProtoSpecT::setup<ipc_file_server::MessageSetup>(*proto);
```

which uses ipc_file_server::MessageSetup defined as follows:

```C++
namespace ipc_file_server{

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<M> &_rsent_msg_ptr,
    std::shared_ptr<M> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
);
    
template <>
void complete_message<ipc_file::ListRequest>(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<ipc_file::ListRequest> &_rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListRequest> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
){
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);
    
    auto msgptr = std::make_shared<ipc_file::ListResponse>(*_rrecv_msg_ptr);
    
    
    fs::path fs_path(msgptr->path.c_str()/*, fs::native*/);
    
    if(fs::exists( fs_path ) and fs::is_directory(fs_path)){
        fs::directory_iterator  it,end;
        
        try{
            it = fs::directory_iterator(fs_path);
        }catch ( const std::exception & ex ){
            it = end;
        }
        
        while(it != end){
            msgptr->node_dq.emplace_back(std::string(it->path().c_str()), static_cast<uint8_t>(is_directory(*it)));
            ++it;
        }
    }
    SOLID_CHECK_ERROR(_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::ListResponse>(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<ipc_file::ListResponse> &_rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListResponse> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
){
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(not _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr);
}


template <>
void complete_message<ipc_file::FileRequest>(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<ipc_file::FileRequest> &_rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileRequest> &_rrecv_msg_ptr,
    ErrorConditionT const &_rerror
){
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);
    
    auto msgptr = std::make_shared<ipc_file::FileResponse>(*_rrecv_msg_ptr);
    
    SOLID_CHECK_ERROR(_rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::FileResponse>(
    frame::mpipc::ConnectionContext &_rctx,
    std::shared_ptr<ipc_file::FileResponse> &_rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileResponse> &_rrecv_msg_ptr,
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
    cout<<"Press any char and ENTER to stop: ";
    char c;
    cin>>c;
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

Well, this is the last tutorial for now.
If solid_frame_mpipc library or solid_frame has captured you attention, start using it and give feedback.
