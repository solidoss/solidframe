#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"

#include "mpipc_request_messages.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters{
    Parameters():port("3333"){}
    
    string          port;
};

//-----------------------------------------------------------------------------
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

namespace{
    ostream& operator<<(ostream &_ros, const ipc_request::Date &_rd){
        _ros<<_rd.year<<'.'<<(int)_rd.month<<'.'<<(int)_rd.day;
        return _ros;
    }
    ostream& operator<<(ostream &_ros, const ipc_request::UserData &_rud){
        _ros<<_rud.full_name<<", "<<_rud.email<<", "<<_rud.country<<", "<<_rud.city<<", "<<_rud.birth_date;
        return _ros;
    }
}
//-----------------------------------------------------------------------------

bool parseArguments(Parameters &_par, int argc, char *argv[]);

//-----------------------------------------------------------------------------
//      main
//-----------------------------------------------------------------------------

int main(int argc, char *argv[]){
    Parameters p;
    
    if(!parseArguments(p, argc, argv)) return 0;
    
    {
        
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
        
        cout<<"Expect lines like:"<<endl;
        cout<<"quit"<<endl;
        cout<<"q"<<endl;
        cout<<"localhost user\\d*"<<endl;
        cout<<"127.0.0.1 [a-z]+_man"<<endl;
        
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
                        recipient.c_str(), req_ptr, lambda, 0
                    );
                }else{
                    cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
                }
            }
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters &_par, int argc, char *argv[]){
    if(argc == 2){
        _par.port = argv[1];
    }
    return true;
}



