// solid/frame/mpipc/mpipcconfiguration.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_MPIPC_MPIPCCONFIGURATION_HPP
#define SOLID_FRAME_MPIPC_MPIPCCONFIGURATION_HPP

#include <vector>
#include "solid/system/function.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/mpipc/mpipcprotocol.hpp"

#include "solid/frame/mpipc/mpipcsocketstub.hpp"



namespace solid{

namespace frame{

namespace aio{
class Resolver;
struct ObjectProxy;
}

namespace mpipc{

enum struct ConnectionValues: size_t{
    SocketEmplacementSize = 128
};

class   Service;
class   MessageWriter;
struct  ConnectionContext;
struct  Configuration;

typedef void (*OnSecureConnectF)(frame::aio::ReactorContext &);
typedef void (*OnSecureAcceptF)(frame::aio::ReactorContext &);

using AddressVectorT                                = std::vector<SocketAddressInet>;

using ResolveCompleteFunctionT                      = FUNCTION<void(AddressVectorT &&)>;
using ConnectionStopFunctionT                       = FUNCTION<void(ConnectionContext &)>;
using ConnectionStartFunctionT                      = FUNCTION<void(ConnectionContext &)>;
using AllocateBufferFunctionT                       = FUNCTION<char*(const uint16_t)>;
using FreeBufferFunctionT                           = FUNCTION<void(char*)>;
using CompressFunctionT                             = FUNCTION<size_t(char*, size_t, ErrorConditionT &)>;
using UncompressFunctionT                           = FUNCTION<size_t(char*, const char*, size_t, ErrorConditionT &)>;
//using ResetSerializerLimitsFunctionT              = FUNCTION<void(ConnectionContext &, serialization::binary::Limits&)>;

using AioSchedulerT                                 = frame::Scheduler<frame::aio::Reactor>;

using ConnectionEnterActiveCompleteFunctionT        = FUNCTION<MessagePointerT(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionEnterPassiveCompleteFunctionT       = FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionSecureHandhakeCompleteFunctionT     = FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionSendRawDataCompleteFunctionT        = FUNCTION<void(ConnectionContext&, ErrorConditionT const&)>;
using ConnectionRecvRawDataCompleteFunctionT        = FUNCTION<void(ConnectionContext&, const char*, size_t&, ErrorConditionT const&)>;
using ConnectionOnEventFunctionT                    = FUNCTION<void(ConnectionContext&, Event&)>;


enum struct ConnectionState{
    Raw,
    Passive,
    Active
};

struct ReaderConfiguration{
    ReaderConfiguration();

    size_t                  max_message_count_multiplex;
    UncompressFunctionT     decompress_fnc;
};

struct WriterConfiguration{
    WriterConfiguration();

    size_t                          max_message_count_multiplex;
    size_t                          max_message_count_response_wait;
    size_t                          max_message_continuous_packet_count;

    CompressFunctionT               inplace_compress_fnc;
    //ResetSerializerLimitsFunctionT    reset_serializer_limits_fnc;
};

struct Configuration{
private:
    Configuration& operator=(const Configuration&) = delete;
    Configuration& operator=(Configuration&&) = default;
public:
    template <class P>
    Configuration(
        AioSchedulerT &_rsch,
        std::shared_ptr<P> &_rprotcol_ptr
    ): pools_mutex_count(16), protocol_ptr(std::static_pointer_cast<Protocol>(_rprotcol_ptr)), pscheduler(&_rsch)
    {
        init();
    }

    Configuration& reset(Configuration &&_ucfg){
        *this = std::move(_ucfg);
        prepare();
        return *this;
    }

    AioSchedulerT & scheduler(){
        return *pscheduler;
    }

    bool isServer()const{
        return server.listener_address_str.size() != 0;
    }

    bool isClient()const{
        return not FUNCTION_EMPTY(client.name_resolve_fnc);
    }

    bool isServerOnly()const{
        return isServer() && !isClient();
    }

    bool isClientOnly()const{
        return !isServer() && isClient();
    }

    char* allocateRecvBuffer(uint8_t &_rbuffer_capacity_kb)const;
    void freeRecvBuffer(char *_pb)const;

    char* allocateSendBuffer(uint8_t &_rbuffer_capacity_kb)const;
    void freeSendBuffer(char *_pb)const;

    size_t connectionReconnectTimeoutSeconds(
        const uint8_t _retry_count,
        const bool _failed_create_connection_object,
        const bool _last_connection_was_connected,
        const bool _last_connection_was_active,
        const bool _last_connection_was_secured
    )const;

    ErrorConditionT check() const;


    size_t connetionReconnectTimeoutSeconds()const;

    size_t                                          pool_max_active_connection_count;
    size_t                                          pool_max_pending_connection_count;
    size_t                                          pool_max_message_queue_size;

    size_t                                          pools_mutex_count;

    ReaderConfiguration                             reader;
    WriterConfiguration                             writer;

    struct Server{
        using ConnectionCreateSocketFunctionT       = FUNCTION<SocketStubPtrT(Configuration const &, frame::aio::ObjectProxy const &, SocketDevice &&, char *)>;
        using ConnectionSecureHandshakeFunctionT    = FUNCTION<void(ConnectionContext &)>;

        Server():listener_port(-1){}

        bool hasSecureConfiguration()const{
            return not secure_any.empty();
        }

        ConnectionCreateSocketFunctionT             connection_create_socket_fnc;
        ConnectionState                             connection_start_state;
        bool                                        connection_start_secure;
        ConnectionStartFunctionT                    connection_start_fnc;
        ConnectionSecureHandshakeFunctionT          connection_on_secure_handshake_fnc;

        Any<>                                       secure_any;

        std::string                                 listener_address_str;
        std::string                                 listener_service_str;

        int listenerPort()const{
            return listener_port;
        }
    private:
        friend class Service;
        int                                         listener_port;

    }                                               server;

    struct Client{
        using ConnectionCreateSocketFunctionT       = FUNCTION<SocketStubPtrT(Configuration const &, frame::aio::ObjectProxy const &, char *)>;
        using AsyncResolveFunctionT                 = FUNCTION<void(const std::string&, ResolveCompleteFunctionT&)>;
        using ConnectionSecureHandshakeFunctionT    = FUNCTION<void(ConnectionContext &)>;

        bool hasSecureConfiguration()const{
            return not secure_any.empty();
        }


        ConnectionCreateSocketFunctionT             connection_create_socket_fnc;

        ConnectionState                             connection_start_state;
        bool                                        connection_start_secure;
        ConnectionStartFunctionT                    connection_start_fnc;

        AsyncResolveFunctionT                       name_resolve_fnc;

        ConnectionSecureHandshakeFunctionT          connection_on_secure_handshake_fnc;
        Any<>                                       secure_any;

    }                                               client;


    size_t                                          connection_reconnect_timeout_seconds;
    uint32_t                                        connection_inactivity_timeout_seconds;
    uint32_t                                        connection_keepalive_timeout_seconds;

    uint32_t                                        connection_inactivity_keepalive_count;  //server error if receives more than inactivity_keepalive_count keep alive
                                                                                            //messages during inactivity_timeout_seconds interval

    uint8_t                                         connection_recv_buffer_start_capacity_kb;
    uint8_t                                         connection_recv_buffer_max_capacity_kb;

    uint8_t                                         connection_send_buffer_start_capacity_kb;
    uint8_t                                         connection_send_buffer_max_capacity_kb;


    ConnectionStopFunctionT                         connection_stop_fnc;
    ConnectionOnEventFunctionT                      connection_on_event_fnc;

    AllocateBufferFunctionT                         connection_recv_buffer_allocate_fnc;
    AllocateBufferFunctionT                         connection_send_buffer_allocate_fnc;

    FreeBufferFunctionT                             connection_recv_buffer_free_fnc;
    FreeBufferFunctionT                             connection_send_buffer_free_fnc;

    ProtocolPointerT                                protocol_ptr;

    Protocol& protocol(){
        return *protocol_ptr;
    }

    const Protocol& protocol()const{
        return *protocol_ptr;
    }
private:
    void init();
    void prepare();
private:
    AioSchedulerT                       *pscheduler;
private:
    friend class Service;
    //friend class MessageWriter;
    Configuration():pscheduler(nullptr){}
};

struct InternetResolverF{
    aio::Resolver       &rresolver;
    std::string         default_service;
    SocketInfo::Family  family;

    InternetResolverF(
        aio::Resolver &_rresolver,
        const char *_default_service,
        SocketInfo::Family  _family = SocketInfo::AnyFamily
    ):  rresolver(_rresolver), default_service(_default_service), family(_family){}

    void operator()(const std::string&, ResolveCompleteFunctionT&);
};

}//namespace mpipc
}//namespace frame
}//namespace solid

#endif
