// solid/frame/ipc/src/ipcservice.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/socketdevice.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/queue.hpp"

#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "solid/system/mutualstore.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/string.hpp"
#include <atomic>

#include "mpipcconnection.hpp"
#include "mpipclistener.hpp"
#include "mpipcutility.hpp"

#include <deque>
#include <vector>

#ifdef SOLID_USE_CPP11
#define CPP11_NS std
#include <unordered_map>
#else
#include "boost/unordered_map.hpp"
#define CPP11_NS boost
#endif

using namespace std;

namespace solid {
namespace frame {
namespace mpipc {
//=============================================================================
typedef CPP11_NS::unordered_map<const char*, size_t, CStringHash, CStringEqual> NameMapT;
typedef Queue<ObjectIdT> ObjectIdQueueT;

enum {
    InnerLinkOrder = 0,
    InnerLinkAsync,
    InnerLinkCount
};

//-----------------------------------------------------------------------------

struct MessageStub : InnerNode<InnerLinkCount> {

    enum {
        CancelableFlag = 1
    };

    MessageStub(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _msgflags)
        : msgbundle(_rmsgptr, _msg_type_idx, _msgflags, _rcomplete_fnc)
        , unique(0)
        , flags(0)
    {
    }

    MessageStub(
        MessageStub&& _rmsg)
        : InnerNode<InnerLinkCount>(std::move(_rmsg))
        , msgbundle(std::move(_rmsg.msgbundle))
        , msgid(_rmsg.msgid)
        , objid(_rmsg.objid)
        , unique(_rmsg.unique)
        , flags(_rmsg.flags)
    {
    }

    MessageStub()
        : unique(0)
        , flags(0)
    {
    }

    bool isCancelable() const
    {
        return (flags & CancelableFlag) != 0;
    }

    void makeCancelable()
    {
        flags |= CancelableFlag;
    }

    void clear()
    {
        msgbundle.clear();
        msgid = MessageId();
        objid = ObjectIdT();
        ++unique;
        flags = 0;
    }

    MessageBundle msgbundle;
    MessageId     msgid;
    ObjectIdT     objid;
    uint32_t      unique;
    uint          flags;
};

//-----------------------------------------------------------------------------

using MessageVectorT         = std::vector<MessageStub>;
using MessageOrderInnerListT = InnerList<MessageVectorT, InnerLinkOrder>;
using MessageCacheInnerListT = InnerList<MessageVectorT, InnerLinkOrder>;
using MessageAsyncInnerListT = InnerList<MessageVectorT, InnerLinkAsync>;

//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, const MessageOrderInnerListT& _rlst)
{
    size_t cnt = 0;
    _rlst.forEach(
        [&_ros, &cnt](size_t _idx, const MessageStub& _rmsg) {
            _ros << _idx << ' ';
            ++cnt;
        });
    SOLID_ASSERT(cnt == _rlst.size());
    return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const MessageAsyncInnerListT& _rlst)
{
    size_t cnt = 0;
    _rlst.forEach(
        [&_ros, &cnt](size_t _idx, const MessageStub& _rmsg) {
            _ros << _idx << ' ';
            ++cnt;
        });
    SOLID_ASSERT(cnt == _rlst.size());
    return _ros;
}

//-----------------------------------------------------------------------------

struct ConnectionPoolStub {
    enum {
        ClosingFlag                = 1,
        FastClosingFlag            = 2,
        MainConnectionStoppingFlag = 4,
        CleanOneShotMessagesFlag   = 8,
        CleanAllMessagesFlag       = 16,
        RestartFlag                = 32,
        MainConnectionActiveFlag   = 64,
    };

    uint32_t               unique;
    uint16_t               pending_connection_count;
    uint16_t               active_connection_count;
    uint16_t               stopping_connection_count;
    std::string            name; //because c_str() pointer is given to connection - name should allways be std::moved
    ObjectIdT              main_connection_id;
    MessageVectorT         msgvec;
    MessageOrderInnerListT msgorder_inner_list;
    MessageCacheInnerListT msgcache_inner_list;
    MessageAsyncInnerListT msgasync_inner_list;

    ObjectIdQueueT conn_waitingq;

    uint8_t        flags;
    uint8_t        retry_connect_count;
    AddressVectorT connect_addr_vec;

    ConnectionPoolStub()
        : unique(0)
        , pending_connection_count(0)
        , active_connection_count(0)
        , stopping_connection_count(0)
        , msgorder_inner_list(msgvec)
        , msgcache_inner_list(msgvec)
        , msgasync_inner_list(msgvec)
        , flags(0)
        , retry_connect_count(0)
    {
    }

    ConnectionPoolStub(
        ConnectionPoolStub&& _rpool)
        : unique(_rpool.unique)
        , pending_connection_count(_rpool.pending_connection_count)
        , active_connection_count(_rpool.active_connection_count)
        , stopping_connection_count(_rpool.stopping_connection_count)
        , name(std::move(_rpool.name))
        , main_connection_id(_rpool.main_connection_id)
        , msgvec(std::move(_rpool.msgvec))
        , msgorder_inner_list(msgvec, _rpool.msgorder_inner_list)
        , msgcache_inner_list(msgvec, _rpool.msgcache_inner_list)
        , msgasync_inner_list(msgvec, _rpool.msgasync_inner_list)
        , conn_waitingq(std::move(_rpool.conn_waitingq))
        , flags(_rpool.flags)
        , retry_connect_count(_rpool.retry_connect_count)
        , connect_addr_vec(std::move(_rpool.connect_addr_vec))
    {
    }

    ~ConnectionPoolStub()
    {
        SOLID_ASSERT(msgcache_inner_list.size() == msgvec.size());
    }

    void clear()
    {
        name.clear();
        main_connection_id = ObjectIdT();
        ++unique;
        pending_connection_count  = 0;
        active_connection_count   = 0;
        stopping_connection_count = 0;
        while (conn_waitingq.size()) {
            conn_waitingq.pop();
        }
        SOLID_ASSERT(msgorder_inner_list.empty());
        SOLID_ASSERT(msgasync_inner_list.empty());
        msgcache_inner_list.clear();
        msgorder_inner_list.clear();
        msgasync_inner_list.clear();
        msgvec.clear();
        msgvec.shrink_to_fit();
        flags               = 0;
        retry_connect_count = 0;
        connect_addr_vec.clear();
        SOLID_ASSERT(msgorder_inner_list.check());
    }

    MessageId insertMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _flags)
    {
        size_t idx;

        if (msgcache_inner_list.size()) {
            idx = msgcache_inner_list.frontIndex();
            msgcache_inner_list.popFront();
        } else {
            idx = msgvec.size();
            msgvec.push_back(MessageStub{});
        }

        MessageStub& rmsgstub(msgvec[idx]);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc);

        //SOLID_ASSERT(rmsgstub.msgbundle.message_ptr.get());

        return MessageId(idx, rmsgstub.unique);
    }

    MessageId pushBackMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _flags)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags);

        msgorder_inner_list.pushBack(msgid.index);

        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushBack(msgid.index);
            idbgx(Debug::mpipc, "msgasync_inner_list " << msgasync_inner_list);
        }

        SOLID_ASSERT(msgorder_inner_list.check());

        return msgid;
    }

    MessageId pushFrontMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _flags)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags);

        msgorder_inner_list.pushFront(msgid.index);

        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(msgid.index);
            idbgx(Debug::mpipc, "msgasync_inner_list " << msgasync_inner_list);
        }

        SOLID_ASSERT(msgorder_inner_list.check());

        return msgid;
    }

    MessageId reinsertFrontMessage(
        MessageId const&          _rmsgid,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _flags)
    {
        MessageStub& rmsgstub(msgvec[_rmsgid.index]);

        SOLID_ASSERT(not rmsgstub.msgbundle.message_ptr and rmsgstub.unique == _rmsgid.unique);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc);

        msgorder_inner_list.pushFront(_rmsgid.index);

        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(_rmsgid.index);
            idbgx(Debug::mpipc, "msgasync_inner_list " << msgasync_inner_list);
        }

        SOLID_ASSERT(msgorder_inner_list.check());
        return _rmsgid;
    }

    void clearAndCacheMessage(const size_t _msg_idx)
    {
        MessageStub& rmsgstub(msgvec[_msg_idx]);
        rmsgstub.clear();
        msgcache_inner_list.pushBack(_msg_idx);
    }

    void cacheFrontMessage()
    {
        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);
        msgcache_inner_list.pushBack(msgorder_inner_list.popFront());

        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void popFrontMessage()
    {
        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.popFront();

        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void eraseMessage(const size_t _msg_idx)
    {
        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        SOLID_ASSERT(msgorder_inner_list.check());
    }
    void clearPopAndCacheMessage(const size_t _msg_idx)
    {
        idbgx(Debug::mpipc, "msgorder_inner_list " << msgorder_inner_list);
        MessageStub& rmsgstub(msgvec[_msg_idx]);

        msgorder_inner_list.erase(_msg_idx);

        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
            msgasync_inner_list.erase(_msg_idx);
        }

        msgcache_inner_list.pushBack(_msg_idx);

        rmsgstub.clear();
        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void cacheMessageId(const MessageId& _rmsgid)
    {
        SOLID_ASSERT(_rmsgid.index < msgvec.size());
        SOLID_ASSERT(msgvec[_rmsgid.index].unique == _rmsgid.unique);
        if (_rmsgid.index < msgvec.size() and msgvec[_rmsgid.index].unique == _rmsgid.unique) {
            msgvec[_rmsgid.index].clear();
            msgcache_inner_list.pushBack(_rmsgid.index);
        }
    }

    bool isMainConnectionStopping() const
    {
        return flags & MainConnectionStoppingFlag;
    }

    void setMainConnectionStopping()
    {
        flags |= MainConnectionStoppingFlag;
    }
    void resetMainConnectionStopping()
    {
        flags &= ~MainConnectionStoppingFlag;
    }

    bool isMainConnectionActive() const
    {
        return flags & MainConnectionActiveFlag;
    }

    void setMainConnectionActive()
    {
        flags |= MainConnectionActiveFlag;
    }
    void resetMainConnectionActive()
    {
        flags &= ~MainConnectionActiveFlag;
    }

    bool hasNoConnection() const
    {
        return pending_connection_count == 0 and active_connection_count == 0 and stopping_connection_count == 0;
    }

    bool isClosing() const
    {
        return flags & ClosingFlag;
    }

    void setClosing()
    {
        flags |= ClosingFlag;
    }
    bool isFastClosing() const
    {
        return flags & FastClosingFlag;
    }

    void setFastClosing()
    {
        flags |= FastClosingFlag;
    }

    bool isServerSide() const
    {
        return name.empty();
    }

    bool isCleaningOneShotMessages() const
    {
        return flags & CleanOneShotMessagesFlag;
    }

    void setCleaningOneShotMessages()
    {
        flags |= CleanOneShotMessagesFlag;
    }

    void resetCleaningOneShotMessages()
    {
        flags &= ~CleanOneShotMessagesFlag;
    }

    bool isCleaningAllMessages() const
    {
        return flags & CleanAllMessagesFlag;
    }

    void setCleaningAllMessages()
    {
        flags |= CleanAllMessagesFlag;
    }

    void resetCleaningAllMessages()
    {
        flags &= ~CleanAllMessagesFlag;
    }

    bool isRestarting() const
    {
        return flags & RestartFlag;
    }

    void setRestarting()
    {
        flags |= RestartFlag;
    }

    void resetRestarting()
    {
        flags &= ~RestartFlag;
    }

    bool hasNoMessage() const
    {
        if (msgorder_inner_list.empty()) {
            return true;
        } else {
            return not msgorder_inner_list.front().msgbundle.message_ptr;
        }
    }

    bool hasAnyMessage() const
    {
        return not hasNoMessage();
    }

    bool isMainConnection(ObjectIdT const& _robjuid) const
    {
        return main_connection_id == _robjuid;
    }

    bool isLastConnection() const
    {
        return (static_cast<size_t>(pending_connection_count) + active_connection_count) == 1;
    }

    bool isFull(const size_t _max_message_queue_size) const
    {
        return msgcache_inner_list.empty() and msgvec.size() >= _max_message_queue_size;
    }

    bool shouldClose() const
    {
        return isClosing() and hasNoMessage();
    }
};

//-----------------------------------------------------------------------------

typedef std::deque<ConnectionPoolStub> ConnectionPoolDequeT;
typedef Stack<size_t>                  SizeStackT;

//-----------------------------------------------------------------------------

struct Service::Data {

    Data(Service& _rsvc)
        : pmtxarr(nullptr)
        , mtxsarrcp(0)
        , config() /*, status(Status::Running)*/
    {
    }

    ~Data()
    {
        delete[] pmtxarr;
    }

    std::mutex& poolMutex(size_t _idx) const
    {
        return pmtxarr[_idx % mtxsarrcp];
    }

    void lockAllConnectionPoolMutexes()
    {
        for (size_t i = 0; i < mtxsarrcp; ++i) {
            pmtxarr[i].lock();
        }
    }

    void unlockAllConnectionPoolMutexes()
    {
        for (size_t i = 0; i < mtxsarrcp; ++i) {
            pmtxarr[i].unlock();
        }
    }

    std::mutex           mtx;
    std::mutex*          pmtxarr;
    size_t               mtxsarrcp;
    NameMapT             namemap;
    ConnectionPoolDequeT pooldq;
    SizeStackT           conpoolcachestk;
    Configuration        config;
};
//=============================================================================

Service::Service(
    UseServiceShell _force_shell)
    : BaseT(_force_shell)
    , d(*(new Data(*this)))
{
}

//! Destructor
Service::~Service()
{
    stop(true);
    delete &d;
}
//-----------------------------------------------------------------------------
Configuration const& Service::configuration() const
{
    return d.config;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::start()
{
    std::unique_lock<std::mutex> lock(d.mtx);
    ErrorConditionT              err = doStart();
    return err;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doStart()
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;

    if (not BaseT::start()) {
        error = error_service_start;
        return error;
    }

    if (configuration().server.listener_address_str.size()) {
        std::string tmp;
        const char* hst_name;
        const char* svc_name;

        size_t off = d.config.server.listener_address_str.rfind(':');
        if (off != std::string::npos) {
            tmp      = d.config.server.listener_address_str.substr(0, off);
            hst_name = tmp.c_str();
            svc_name = d.config.server.listener_address_str.c_str() + off + 1;
            if (!svc_name[0]) {
                svc_name = d.config.server.listener_service_str.c_str();
            }
        } else {
            hst_name = d.config.server.listener_address_str.c_str();
            svc_name = d.config.server.listener_service_str.c_str();
        }

        ResolveData  rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);
        SocketDevice sd;

        if (not rd.empty()) {
            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), 2000);
        }

        if (sd.ok()) {

            SocketAddress local_address;

            sd.localAddress(local_address);

            d.config.server.listener_port = local_address.port();

            DynamicPointer<aio::Object> objptr(new Listener(sd));

            ObjectIdT conuid = d.config.scheduler().startObject(objptr, *this, make_event(GenericEvents::Start), error);
            (void)conuid;
            if (error) {
                return error;
            }
        } else {
            error = error_service_start_listener;
            return error;
        }
    }

    if (d.config.pools_mutex_count > d.mtxsarrcp) {
        delete[] d.pmtxarr;
        d.pmtxarr   = new std::mutex[d.config.pools_mutex_count];
        d.mtxsarrcp = d.config.pools_mutex_count;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::reconfigure(Configuration&& _ucfg)
{

    vdbgx(Debug::mpipc, this);

    BaseT::stop(true); //block until all objects are destroyed

    unique_lock<std::mutex> lock(d.mtx);

    {
        ErrorConditionT error;

        error = _ucfg.check();

        if (error)
            return error;

        d.config.reset(std::move(_ucfg));
    }
    return doStart();
}
//-----------------------------------------------------------------------------
size_t Service::doPushNewConnectionPool()
{

    vdbgx(Debug::mpipc, this);

    d.lockAllConnectionPoolMutexes();

    for (size_t i = 0; i < d.mtxsarrcp; ++i) {
        size_t idx = d.pooldq.size();
        d.pooldq.push_back(ConnectionPoolStub());
        d.conpoolcachestk.push(d.mtxsarrcp - idx - 1);
    }
    d.unlockAllConnectionPoolMutexes();
    size_t idx = d.conpoolcachestk.top();
    d.conpoolcachestk.pop();
    return idx;
}

//-----------------------------------------------------------------------------

struct OnRelsolveF {

    Manager&  rm;
    ObjectIdT objuid;
    Event     event;

    OnRelsolveF(
        Manager&         _rm,
        const ObjectIdT& _robjuid,
        const Event&     _revent)
        : rm(_rm)
        , objuid(_robjuid)
        , event(_revent)
    {
    }

    void operator()(AddressVectorT&& _raddrvec)
    {
        idbgx(Debug::mpipc, "OnResolveF(addrvec of size " << _raddrvec.size() << ")");
        event.any().reset(ResolveMessage(std::move(_raddrvec)));
        rm.notify(objuid, std::move(event));
    }
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
ErrorConditionT Service::doSendMessage(
    const char*               _recipient_name,
    const RecipientId&        _rrecipient_id_in,
    MessagePointerT&          _rmsgptr,
    MessageCompleteFunctionT& _rcomplete_fnc,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    ulong                     _flags)
{

    vdbgx(Debug::mpipc, this);

    solid::ErrorConditionT error;
    size_t                 pool_idx;
    uint32_t               unique;
    bool                   check_uid = false;

    unique_lock<std::mutex> lock(d.mtx);

    if (not isRunning()) {
        edbgx(Debug::mpipc, this << " service stopping");
        error = error_service_stopping;
        return error;
    }

    if (!_rmsgptr) {
        error = error_service_message_null;
        return error;
    }

    if (Message::is_response(_flags)) {
        //message state should be isOnPeer
        if (!_rmsgptr->isOnPeer()) {
            error = error_service_message_state;
            return error;
        }
        if (Message::is_request(_flags)) {
            error = error_service_message_flags;
            return error;
        }
    } else {
        if (_rmsgptr->isOnPeer()) {
            error = error_service_message_state;
            return error;
        }
    }

    const size_t msg_type_idx = configuration().protocol().typeIndex(_rmsgptr.get());

    if (msg_type_idx == 0) {
        edbgx(Debug::mpipc, this << " message type not registered");
        error = error_service_message_unknown_type;
        return error;
    }

    if (_rrecipient_id_in.isValidConnection()) {
        SOLID_ASSERT(_precipient_id_out == nullptr);
        //directly send the message to a connection object
        return doSendMessageToConnection(
            _rrecipient_id_in,
            _rmsgptr,
            msg_type_idx,
            _rcomplete_fnc,
            _pmsgid_out,
            _flags);
    } else if (_recipient_name) {
        NameMapT::const_iterator it = d.namemap.find(_recipient_name);

        if (it != d.namemap.end()) {
            pool_idx = it->second;
        } else {
            if (configuration().isServerOnly()) {
                edbgx(Debug::mpipc, this << " request for name resolve for a server only configuration");
                error = error_service_server_only;
                return error;
            }

            return this->doSendMessageToNewPool(
                _recipient_name, _rmsgptr, msg_type_idx,
                _rcomplete_fnc, _precipient_id_out, _pmsgid_out, _flags);
        }
    } else if (
        _rrecipient_id_in.poolid.index < d.pooldq.size()) {
        //we cannot check the uid right now because we need a lock on the pool's mutex
        check_uid = true;
        pool_idx  = _rrecipient_id_in.poolid.index;
        unique    = _rrecipient_id_in.poolid.unique;
    } else {
        edbgx(Debug::mpipc, this << " recipient does not exist");
        error = error_service_unknown_recipient;
        return error;
    }

    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    if (check_uid && rpool.unique != unique) {
        //failed uid check
        edbgx(Debug::mpipc, this << " connection pool does not exist");
        error = error_service_unknown_pool;
        return error;
    }

    if (rpool.isClosing()) {
        edbgx(Debug::mpipc, this << " connection pool is stopping");
        error = error_service_pool_stopping;
        return error;
    }

    if (rpool.isFull(configuration().pool_max_message_queue_size)) {
        edbgx(Debug::mpipc, this << " connection pool is full");
        error = error_service_pool_full;
        return error;
    }

    if (_precipient_id_out) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_idx, rpool.unique);
    }

    //At this point we can fetch the message from user's pointer
    //because from now on we can call complete on the message
    const MessageId msgid = rpool.pushBackMessage(_rmsgptr, msg_type_idx, _rcomplete_fnc, _flags);

    if (_pmsgid_out) {

        MessageStub& rmsgstub(rpool.msgvec[msgid.index]);

        rmsgstub.makeCancelable();

        *_pmsgid_out = msgid;
        idbgx(Debug::mpipc, this << " set message id to " << *_pmsgid_out);
    }

    bool success = false;

    if (
        rpool.isCleaningOneShotMessages() and Message::is_one_shot(_flags)) {
        success = manager().notify(
            rpool.main_connection_id,
            Connection::eventNewMessage(msgid));

        SOLID_ASSERT(success);
    }

    if (
        not success and Message::is_synchronous(_flags) and
        //      rpool.main_connection_id.isValid() and
        rpool.isMainConnectionActive()) {
        success = manager().notify(
            rpool.main_connection_id,
            Connection::eventNewMessage());
        SOLID_ASSERT(success);
    }

    if (not success and not Message::is_synchronous(_flags)) {
        success = doTryNotifyPoolWaitingConnection(pool_idx);
    }

    if (not success) {
        doTryCreateNewConnectionForPool(pool_idx, error);
        error.clear();
    }

    if (not success) {
        wdbgx(Debug::mpipc, this << " no connection notified about the new message");
    }

    return error;
}

//-----------------------------------------------------------------------------

ErrorConditionT Service::doSendMessageToConnection(
    const RecipientId&        _rrecipient_id_in,
    MessagePointerT&          _rmsgptr,
    const size_t              _msg_type_idx,
    MessageCompleteFunctionT& _rcomplete_fnc,
    MessageId*                _pmsgid_out,
    ulong                     _flags)
{
    //d.mtx must be locked

    vdbgx(Debug::mpipc, this);

    if (not _rrecipient_id_in.isValidPool()) {
        SOLID_ASSERT(false);
        return error_service_unknown_connection;
    }

    const size_t pool_idx = _rrecipient_id_in.poolId().index;

    if (pool_idx >= d.pooldq.size() or d.pooldq[pool_idx].unique != _rrecipient_id_in.poolId().unique) {
        return error_service_unknown_connection;
    }

    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool = d.pooldq[pool_idx];
    solid::ErrorConditionT  error;
    const bool              is_server_side_pool = rpool.isServerSide(); //unnamed pool has a single connection

    MessageId msgid;

    bool success = false;

    if (is_server_side_pool) {
        //for a server pool we want to enque messages in the pool
        //
        msgid   = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags | MessageFlags::OneShotSend);
        success = manager().notify(
            _rrecipient_id_in.connectionId(),
            Connection::eventNewMessage());
    } else {
        msgid   = rpool.insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags | MessageFlags::OneShotSend);
        success = manager().notify(
            _rrecipient_id_in.connectionId(),
            Connection::eventNewMessage(msgid));
    }

    if (success) {
        if (_pmsgid_out) {
            *_pmsgid_out          = msgid;
            MessageStub& rmsgstub = rpool.msgvec[msgid.index];
            rmsgstub.makeCancelable();
        }
    } else if (is_server_side_pool) {
        rpool.clearPopAndCacheMessage(msgid.index);
        error = error_service_unknown_connection;
    } else {
        rpool.clearAndCacheMessage(msgid.index);
        error = error_service_unknown_connection;
    }

    return error;
}

//-----------------------------------------------------------------------------

ErrorConditionT Service::doSendMessageToNewPool(
    const char*               _recipient_name,
    MessagePointerT&          _rmsgptr,
    const size_t              _msg_type_idx,
    MessageCompleteFunctionT& _rcomplete_fnc,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    ulong                     _flags)
{

    vdbgx(Debug::mpipc, this);

    solid::ErrorConditionT error;
    size_t                 pool_idx;

    if (d.conpoolcachestk.size()) {
        pool_idx = d.conpoolcachestk.top();
        d.conpoolcachestk.pop();
    } else {
        pool_idx = this->doPushNewConnectionPool();
    }

    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    rpool.name = _recipient_name;

    MessageId msgid = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags);

    if (not doTryCreateNewConnectionForPool(pool_idx, error)) {
        edbgx(Debug::mpipc, this << " Starting Session: " << error.message());
        rpool.popFrontMessage();
        rpool.clear();
        d.conpoolcachestk.push(pool_idx);
        return error;
    }

    d.namemap[rpool.name.c_str()] = pool_idx;

    if (_precipient_id_out) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_idx, rpool.unique);
    }

    if (_pmsgid_out) {
        MessageStub& rmsgstub = rpool.msgvec[msgid.index];

        *_pmsgid_out = MessageId(msgid.index, rmsgstub.unique);
        rmsgstub.makeCancelable();
    }

    return error;
}
//-----------------------------------------------------------------------------
// doTryPushMessageToConnection will accept a message when:
// there is space in the sending queue and
//  either the message is not waiting for response or there is space in the waiting response message queue
bool Service::doTryPushMessageToConnection(
    Connection&      _rcon,
    ObjectIdT const& _robjuid,
    const size_t     _pool_idx,
    const size_t     _msg_idx)
{

    vdbgx(Debug::mpipc, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(d.pooldq[_pool_idx]);
    MessageStub&        rmsgstub                = rpool.msgvec[_msg_idx];
    const bool          message_is_asynchronous = Message::is_asynchronous(rmsgstub.msgbundle.message_flags);
    const bool          message_is_null         = not rmsgstub.msgbundle.message_ptr;
    bool                success                 = false;

    SOLID_ASSERT(not Message::is_canceled(rmsgstub.msgbundle.message_flags));

    if (not rmsgstub.msgbundle.message_ptr) {
        return false;
    }

    if (rmsgstub.isCancelable()) {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId(_msg_idx, rmsgstub.unique));

        if (success and not message_is_null) {

            rmsgstub.objid = _robjuid;

            rpool.msgorder_inner_list.erase(_msg_idx);

            if (message_is_asynchronous) {
                rpool.msgasync_inner_list.erase(_msg_idx);
            }
        }

    } else {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId());

        if (success and not message_is_null) {

            rpool.msgorder_inner_list.erase(_msg_idx);

            if (message_is_asynchronous) {
                rpool.msgasync_inner_list.erase(_msg_idx);
            }

            rpool.msgcache_inner_list.pushBack(_msg_idx);
            rmsgstub.clear();
        }
    }
    return success;
}
//-----------------------------------------------------------------------------
// doTryPushMessageToConnection will accept a message when:
// there is space in the sending queue and
//  either the message is not waiting for response or there is space in the waiting response message queue
bool Service::doTryPushMessageToConnection(
    Connection&      _rcon,
    ObjectIdT const& _robjuid,
    const size_t     _pool_idx,
    const MessageId& _rmsg_id)
{

    vdbgx(Debug::mpipc, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(d.pooldq[_pool_idx]);

    MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
    bool         success  = false;

    SOLID_ASSERT(not Message::is_canceled(rmsgstub.msgbundle.message_flags));

    if (rmsgstub.isCancelable()) {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, _rmsg_id);

        if (success) {
            rmsgstub.objid = _robjuid;
        }
    } else {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId());

        if (success) {
            rpool.msgcache_inner_list.pushBack(_rmsg_id.index);
            rmsgstub.clear();
        }
    }
    return success;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::pollPoolForUpdates(
    Connection&      _rconnection,
    ObjectIdT const& _robjuid,
    MessageId const& _rcompleted_msgid)
{

    vdbgx(Debug::mpipc, this << " " << &_rconnection);

    const size_t            pool_idx = _rconnection.poolId().index;
    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    ErrorConditionT error;

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_unknown_pool;
        return error;
    }

    if (rpool.isMainConnectionStopping() and rpool.main_connection_id != _robjuid) {

        idbgx(Debug::mpipc, this << ' ' << &_rconnection << " switch message main connection from " << rpool.main_connection_id << " to " << _robjuid);

        manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());

        rpool.main_connection_id = _robjuid;
        rpool.resetMainConnectionStopping();
        rpool.setMainConnectionActive();
    }

    if (_rcompleted_msgid.isValid()) {
        rpool.cacheMessageId(_rcompleted_msgid);
    }

    if (rpool.isFastClosing()) {
        idbgx(Debug::mpipc, this << ' ' << &_rconnection << " pool is FastClosing");
        error = error_service_pool_stopping;
        return error;
    }

    if (_rconnection.isWriterEmpty() and rpool.shouldClose()) {
        idbgx(Debug::mpipc, this << ' ' << &_rconnection << " pool is DelayClosing");
        error = error_service_pool_stopping;
        return error;
    }

    idbgx(Debug::mpipc, this << ' ' << &_rconnection << " messages in pool: " << rpool.msgorder_inner_list.size());

    bool       connection_may_handle_more_messages = not _rconnection.isFull(configuration());
    const bool connection_can_handle_synchronous_messages{_robjuid == rpool.main_connection_id};

    //We need to push as many messages as we can to the connection
    //in order to handle eficiently the situation with multiple small messages.

    if (_rconnection.pendingMessageVector().size()) {
        //connection has pending messages
        //fetch as many as we can
        size_t count = 0;

        for (const auto& rmsgid : _rconnection.pendingMessageVector()) {
            if (rmsgid.index < rpool.msgvec.size() and rmsgid.unique == rpool.msgvec[rmsgid.index].unique) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(_rconnection, _robjuid, pool_idx, rmsgid);
                if (connection_may_handle_more_messages) {
                } else {
                    break;
                }
            } else {
                SOLID_ASSERT(false);
            }
            ++count;
        }
        if (count) {
            _rconnection.pendingMessageVectorEraseFirst(count);
        }
    }

    if (connection_can_handle_synchronous_messages) {
        //use the order inner queue
        while (rpool.msgorder_inner_list.size() and connection_may_handle_more_messages) {
            connection_may_handle_more_messages = doTryPushMessageToConnection(
                _rconnection,
                _robjuid,
                pool_idx,
                rpool.msgorder_inner_list.frontIndex());
        }

    } else {

        //use the async inner queue
        while (rpool.msgasync_inner_list.size() and connection_may_handle_more_messages) {
            connection_may_handle_more_messages = doTryPushMessageToConnection(
                _rconnection,
                _robjuid,
                pool_idx,
                rpool.msgasync_inner_list.frontIndex());
        }
    }
    //a connection will either be in conn_waitingq
    //or it will call pollPoolForUpdates asap.
    //this is because we need to be able to notify connection about
    //pool force close imeditely
    if (not _rconnection.isInPoolWaitingQueue()) {
        rpool.conn_waitingq.push(_robjuid);
        _rconnection.setInPoolWaitingQueue();
    }

    return error;
}
//-----------------------------------------------------------------------------
void Service::rejectNewPoolMessage(Connection const& _rconnection)
{

    vdbgx(Debug::mpipc, this);

    const size_t            pool_idx = _rconnection.poolId().index;
    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique)
        return;

    doTryNotifyPoolWaitingConnection(pool_idx);
}
//-----------------------------------------------------------------------------
bool Service::doTryNotifyPoolWaitingConnection(const size_t _pool_index)
{

    vdbgx(Debug::mpipc, this);

    ConnectionPoolStub& rpool(d.pooldq[_pool_index]);
    bool                success = false;

    //we were not able to handle the message, try notify another connection
    while (not success and rpool.conn_waitingq.size()) {
        //a connection is waiting for something to send
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        success = manager().notify(
            objuid,
            Connection::eventNewMessage());
    }
    return success;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doDelayCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT         error;
    const size_t            pool_idx = _rrecipient_id.poolId().index;
    unique_lock<std::mutex> lock(d.mtx);
    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();

    if (rpool.name.size()) {
        d.namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, 0);
    (void)msgid;

    //notify all waiting connections about the new message
    while (rpool.conn_waitingq.size()) {
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            objuid,
            Connection::eventNewMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doForceCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT         error;
    const size_t            pool_idx = _rrecipient_id.poolId().index;
    unique_lock<std::mutex> lock(d.mtx);
    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();
    rpool.setFastClosing();

    if (rpool.name.size()) {
        d.namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, 0 | MessageFlags::Synchronous);
    (void)msgid;

    //no reason to cancel all messages - they'll be handled on connection stop.
    //notify all waiting connections about the new message
    while (rpool.conn_waitingq.size()) {
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            objuid,
            Connection::eventNewMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::cancelMessage(RecipientId const& _rrecipient_id, MessageId const& _rmsg_id)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    const size_t    pool_idx = _rrecipient_id.poolId().index;
    //unique_lock<std::mutex>   lock(d.mtx);
    unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));
    ConnectionPoolStub&     rpool(d.pooldq[pool_idx]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    if (_rmsg_id.index < rpool.msgvec.size() and rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
        bool         success  = false;

        if (Message::is_canceled(rmsgstub.msgbundle.message_flags)) {
            error = error_service_message_already_canceled;
        } else {

            if (rmsgstub.objid.isValid()) { //message handled by a connection

                vdbgx(Debug::mpipc, this << " message " << _rmsg_id << " from pool " << pool_idx << " is handled by connection " << rmsgstub.objid);

                SOLID_ASSERT(not rmsgstub.msgbundle.message_ptr);

                rmsgstub.msgbundle.message_flags |= MessageFlags::Canceled;

                success = manager().notify(
                    rmsgstub.objid,
                    Connection::eventCancelConnMessage(rmsgstub.msgid));

                if (success) {
                    rmsgstub.clear();
                    rpool.msgcache_inner_list.pushBack(_rmsg_id.index);
                } else {
                    rmsgstub.msgid = MessageId();
                    rmsgstub.objid = ObjectIdT();
                    SOLID_THROW("Lost message");
                    error = error_service_message_lost;
                }
            }

            if (rmsgstub.msgbundle.message_ptr) {

                vdbgx(Debug::mpipc, this << " message " << _rmsg_id << " from pool " << pool_idx << " not handled by any connection");

                rmsgstub.msgbundle.message_flags |= MessageFlags::Canceled;

                success = manager().notify(
                    rpool.main_connection_id,
                    Connection::eventCancelPoolMessage(_rmsg_id));

                if (success) {
                    vdbgx(Debug::mpipc, this << " message " << _rmsg_id << " from pool " << pool_idx << " sent for canceling to " << rpool.main_connection_id);
                    //erase/unlink the message from any list
                    if (rpool.msgorder_inner_list.contains(_rmsg_id.index)) {
                        rpool.msgorder_inner_list.erase(_rmsg_id.index);
                        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
                            SOLID_ASSERT(rpool.msgasync_inner_list.contains(_rmsg_id.index));
                            rpool.msgasync_inner_list.erase(_rmsg_id.index);
                        }
                    }
                } else {
                    SOLID_THROW("Message Cancel connection not available");
                }
            }
        }
    } else {
        error = error_service_unknown_message;
    }
    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyEnterActiveState(
    RecipientId const&                       _rrecipient_id,
    ConnectionEnterActiveCompleteFunctionT&& _ucomplete_fnc,
    const size_t                             _send_buffer_capacity)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity));

    if (not success) {
        wdbgx(Debug::mpipc, this << " failed notify enter active event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyStartSecureHandshake(
    RecipientId const&                          _rrecipient_id,
    ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventStartSecure(std::move(_ucomplete_fnc)));

    if (not success) {
        wdbgx(Debug::mpipc, this << " failed notify start secure event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyEnterPassiveState(
    RecipientId const&                        _rrecipient_id,
    ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterPassive(std::move(_ucomplete_fnc)));

    if (not success) {
        wdbgx(Debug::mpipc, this << " failed notify enter passive event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifySendRawData(
    RecipientId const&                       _rrecipient_id,
    ConnectionSendRawDataCompleteFunctionT&& _ucomplete_fnc,
    std::string&&                            _rdata)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventSendRaw(std::move(_ucomplete_fnc), std::move(_rdata)));

    if (not success) {
        wdbgx(Debug::mpipc, this << " failed notify send raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyRecvRawData(
    RecipientId const&                       _rrecipient_id,
    ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
{

    vdbgx(Debug::mpipc, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventRecvRaw(std::move(_ucomplete_fnc)));

    if (not success) {
        wdbgx(Debug::mpipc, this << " failed notify recv raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
bool Service::fetchMessage(Connection& _rcon, ObjectIdT const& _robjuid, MessageId const& _rmsg_id)
{

    vdbgx(Debug::mpipc, this);

    unique_lock<std::mutex> lock2(d.poolMutex(_rcon.poolId().index));
    const size_t            pool_index = _rcon.poolId().index;
    ConnectionPoolStub&     rpool(d.pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() and rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        return doTryPushMessageToConnection(_rcon, _robjuid, pool_index, _rmsg_id.index);
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::fetchCanceledMessage(Connection const& _rcon, MessageId const& _rmsg_id, MessageBundle& _rmsg_bundle)
{

    vdbgx(Debug::mpipc, this);

    unique_lock<std::mutex> lock2(d.poolMutex(_rcon.poolId().index));
    const size_t            pool_index = _rcon.poolId().index;
    ConnectionPoolStub&     rpool(d.pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() and rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
        SOLID_ASSERT(Message::is_canceled(rmsgstub.msgbundle.message_flags));
        SOLID_ASSERT(not rpool.msgorder_inner_list.contains(_rmsg_id.index));
        _rmsg_bundle = std::move(rmsgstub.msgbundle);
        rmsgstub.clear();
        rpool.msgcache_inner_list.pushBack(_rmsg_id.index);
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::closeConnection(RecipientId const& _rrecipient_id)
{

    vdbgx(Debug::mpipc, this);

    return manager().notify(
        _rrecipient_id.connectionId(),
        make_event(GenericEvents::Kill));
}
//-----------------------------------------------------------------------------
bool Service::connectionStopping(
    Connection&      _rcon,
    ObjectIdT const& _robjuid,
    ulong&           _rseconds_to_wait,
    MessageId&       _rmsg_id,
    MessageBundle&   _rmsg_bundle,
    Event&           _revent_context,
    ErrorConditionT& _rerror)
{

    vdbgx(Debug::mpipc, this);

    const size_t            pool_index = _rcon.poolId().index;
    unique_lock<std::mutex> lock2(d.poolMutex(pool_index));
    ConnectionPoolStub&     rpool(d.pooldq[pool_index]);

    _rseconds_to_wait = 0;
    _rmsg_bundle.clear();

    SOLID_ASSERT(rpool.unique == _rcon.poolId().unique);
    if (rpool.unique != _rcon.poolId().unique)
        return false;

    idbgx(Debug::mpipc, this << ' ' << pool_index << " active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);

    if (not rpool.isMainConnection(_robjuid)) {
        return doNonMainConnectionStopping(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (not rpool.isLastConnection()) {
        return doMainConnectionStoppingNotLast(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isCleaningOneShotMessages()) {
        return doMainConnectionStoppingCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isCleaningAllMessages()) {
        return doMainConnectionStoppingCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isRestarting() and isRunning()) {
        return doMainConnectionRestarting(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (not rpool.isFastClosing() and not rpool.isServerSide() and isRunning() and _rerror != error_connection_resolve) {
        return doMainConnectionStoppingPrepareCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else {
        return doMainConnectionStoppingPrepareCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    }
}
//-----------------------------------------------------------------------------
bool Service::doNonMainConnectionStopping(
    Connection& _rcon, ObjectIdT const& _robjuid,
    ulong&         _rseconds_to_wait,
    MessageId&     _rmsg_id,
    MessageBundle& _rmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        SOLID_ASSERT(not _rcon.isServer());
        SOLID_ASSERT(rpool.pending_connection_count >= 0);
        --rpool.pending_connection_count;
    }

    ++rpool.stopping_connection_count;

    if (rpool.isLastConnection() and rpool.isMainConnectionStopping()) {
        manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());
    }

    if (not rpool.isFastClosing()) {
        doFetchResendableMessagesFromConnection(_rcon);
        ErrorConditionT error;
        doTryCreateNewConnectionForPool(pool_index, error);
    }

    return true; //the connection can call connectionStop asap
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingNotLast(
    Connection& _rcon, ObjectIdT const& /*_robjuid*/,
    ulong&      _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle& /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    //is main connection but is not the last one
    _rseconds_to_wait = 10;
    rpool.setMainConnectionStopping();
    rpool.resetMainConnectionActive();

    if (not rpool.isFastClosing()) {
        doFetchResendableMessagesFromConnection(_rcon);
    }

    return false;
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingCleanOneShot(
    Connection& _rcon, ObjectIdT const& _robjuid,
    ulong&         _rseconds_to_wait,
    MessageId&     _rmsg_id,
    MessageBundle& _rmsg_bundle,
    Event&         _revent_context,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    size_t* pmsgidx = _revent_context.any().cast<size_t>();
    SOLID_ASSERT(pmsgidx);
    const size_t crtmsgidx = *pmsgidx;

    if (crtmsgidx != InvalidIndex()) {

        MessageStub& rmsgstub = rpool.msgvec[crtmsgidx];

        *pmsgidx = rpool.msgorder_inner_list.previousIndex(crtmsgidx);

        if (rpool.msgorder_inner_list.size() != 1) {
            SOLID_ASSERT(rpool.msgorder_inner_list.contains(crtmsgidx));
        } else if (rpool.msgorder_inner_list.size() == 1) {
            SOLID_ASSERT(rpool.msgorder_inner_list.frontIndex() == crtmsgidx);
        } else {
            SOLID_ASSERT(false);
        }

        if (rmsgstub.msgbundle.message_ptr.get() and Message::is_one_shot(rmsgstub.msgbundle.message_flags)) {
            _rmsg_bundle = std::move(rmsgstub.msgbundle);
            _rmsg_id     = MessageId(crtmsgidx, rmsgstub.unique);
            rpool.clearPopAndCacheMessage(crtmsgidx);
        } else if (not rmsgstub.msgbundle.message_ptr and rpool.msgorder_inner_list.size() == 1) {
            _rmsg_bundle = std::move(rmsgstub.msgbundle);
            _rmsg_id     = MessageId(crtmsgidx, rmsgstub.unique);
            rpool.clearPopAndCacheMessage(crtmsgidx);
        }
        return false;
    } else {
        rpool.resetCleaningOneShotMessages();
        rpool.setRestarting();
        if (rpool.connect_addr_vec.empty()) {
            _rseconds_to_wait = configuration().connectionReconnectTimeoutSeconds(
                rpool.retry_connect_count,
                false,
                _rcon.isConnected(),
                _rcon.isActiveState(),
                _rcon.isSecured());
        } else {
            _rseconds_to_wait = 0;
        }
        return false;
    }
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingCleanAll(
    Connection& _rcon, ObjectIdT const& _robjuid,
    ulong&         _rseconds_to_wait,
    MessageId&     _rmsg_id,
    MessageBundle& _rmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    if (rpool.msgorder_inner_list.size()) {
        const size_t msgidx = rpool.msgorder_inner_list.frontIndex();
        {
            MessageStub& rmsgstub = rpool.msgorder_inner_list.front();
            _rmsg_bundle          = std::move(rmsgstub.msgbundle);
            _rmsg_id              = MessageId(msgidx, rmsgstub.unique);
        }
        rpool.clearPopAndCacheMessage(msgidx);
        return false;
    } else {
        if (_rcon.isActiveState()) {
            --rpool.active_connection_count;
        } else {
            SOLID_ASSERT(rpool.pending_connection_count >= 0);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;
        rpool.resetCleaningAllMessages();
        return true; //TODO: maybe we can return false
    }
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingPrepareCleanOneShot(
    Connection& _rcon, ObjectIdT const& /*_robjuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle& /*_rmsg_bundle*/,
    Event& _revent_context,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    //the last connection
    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    doFetchResendableMessagesFromConnection(_rcon);

    rpool.resetMainConnectionActive();

    if (rpool.msgorder_inner_list.size()) {
        rpool.setCleaningOneShotMessages();

        _revent_context.any().reset(rpool.msgorder_inner_list.frontIndex());

        return false;
    } else {
        if (_rcon.isActiveState()) {
            --rpool.active_connection_count;
        } else {
            SOLID_ASSERT(rpool.pending_connection_count >= 0);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;
        return true; //the connection can call connectionStop asap
    }
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingPrepareCleanAll(
    Connection& _rcon, ObjectIdT const& /*_robjuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle& /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    //the last connection - fast closing or server side
    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    rpool.setCleaningAllMessages();
    rpool.resetMainConnectionActive();
    rpool.setFastClosing();
    rpool.setClosing();

    return false;
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionRestarting(
    Connection& _rcon, ObjectIdT const& _robjuid,
    ulong& _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle& /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
    )
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t        pool_index = _rcon.poolId().index;
    ConnectionPoolStub& rpool(d.pooldq[pool_index]);

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        SOLID_ASSERT(rpool.pending_connection_count >= 0);
        --rpool.pending_connection_count;
    }

    ++rpool.stopping_connection_count;
    rpool.main_connection_id = ObjectIdT();

    if (_rcon.isConnected()) {
        rpool.retry_connect_count = 0;
    }

    if (rpool.hasAnyMessage()) {

        ErrorConditionT error;
        bool            success = false;
        bool            tried   = false;

        while (rpool.conn_waitingq.size()) {
            rpool.conn_waitingq.pop();
        }

        if ((rpool.retry_connect_count & 1) == 0) {
            success = doTryCreateNewConnectionForPool(pool_index, error);
            tried   = true;
        }

        ++rpool.retry_connect_count;

        if (not success) {
            if (_rcon.isActiveState()) {
                ++rpool.active_connection_count;
            } else {
                SOLID_ASSERT(not _rcon.isServer());
                ++rpool.pending_connection_count;
            }

            --rpool.stopping_connection_count;
            rpool.main_connection_id = _robjuid;

            _rseconds_to_wait = configuration().connectionReconnectTimeoutSeconds(
                rpool.retry_connect_count,
                tried,
                _rcon.isConnected(),
                _rcon.isActiveState(),
                _rcon.isSecured());
            return false;
        }
    }
    return true;
}
//-----------------------------------------------------------------------------
void Service::connectionStop(Connection const& _rcon)
{

    idbgx(Debug::mpipc, this << ' ' << &_rcon);

    const size_t            pool_index = _rcon.poolId().index;
    unique_lock<std::mutex> lock(d.mtx);
    unique_lock<std::mutex> lock2(d.poolMutex(pool_index));
    ConnectionPoolStub&     rpool(d.pooldq[pool_index]);

    SOLID_ASSERT(rpool.unique == _rcon.poolId().unique);
    if (rpool.unique != _rcon.poolId().unique)
        return;

    --rpool.stopping_connection_count;
    rpool.resetRestarting();

    if (rpool.hasNoConnection()) {

        SOLID_ASSERT(rpool.msgorder_inner_list.empty());
        d.conpoolcachestk.push(pool_index);

        if (rpool.name.size() and not rpool.isClosing()) { //closign pools are already unregistered from namemap
            d.namemap.erase(rpool.name.c_str());
        }

        rpool.clear();
    }
}
//-----------------------------------------------------------------------------
bool Service::doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT& _rerror)
{

    vdbgx(Debug::mpipc, this);

    ConnectionPoolStub& rpool(d.pooldq[_pool_index]);

    if (
        rpool.active_connection_count < configuration().pool_max_active_connection_count and rpool.pending_connection_count == 0 and rpool.hasAnyMessage() and rpool.conn_waitingq.size() < rpool.msgorder_inner_list.size() and isRunning()) {

        idbgx(Debug::mpipc, this << " try create new connection in pool " << rpool.active_connection_count << " pending connections " << rpool.pending_connection_count);

        DynamicPointer<aio::Object> objptr(new_connection(configuration(), ConnectionPoolId(_pool_index, rpool.unique), rpool.name));

        ObjectIdT conuid = d.config.scheduler().startObject(objptr, *this, make_event(GenericEvents::Start), _rerror);

        if (!_rerror) {

            idbgx(Debug::mpipc, this << " Success starting Connection Pool object: " << conuid.index << ',' << conuid.unique);

            ++rpool.pending_connection_count;

            if (rpool.main_connection_id.isInvalid()) {
                rpool.main_connection_id = conuid;
            }

            if (rpool.connect_addr_vec.empty()) {

                ResolveCompleteFunctionT cbk(OnRelsolveF(manager(), conuid, Connection::eventResolve()));

                configuration().client.name_resolve_fnc(rpool.name, cbk);

            } else {
                //use the rest of the already resolved addresses
                Event event = Connection::eventResolve();

                event.any().reset(ResolveMessage(std::move(rpool.connect_addr_vec)));

                manager().notify(conuid, std::move(event));
            }

            return true;
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
void Service::forwardResolveMessage(ConnectionPoolId const& _rpoolid, Event& _revent)
{

    vdbgx(Debug::mpipc, this);

    ResolveMessage* presolvemsg = _revent.any().cast<ResolveMessage>();
    ErrorConditionT error;

    if (presolvemsg->addrvec.size()) {
        unique_lock<std::mutex> lock(d.poolMutex(_rpoolid.index));
        ConnectionPoolStub&     rpool(d.pooldq[_rpoolid.index]);

        if (rpool.pending_connection_count < configuration().pool_max_pending_connection_count) {

            DynamicPointer<aio::Object> objptr(new_connection(configuration(), _rpoolid, rpool.name));

            ObjectIdT conuid = d.config.scheduler().startObject(objptr, *this, std::move(_revent), error);

            if (!error) {
                ++rpool.pending_connection_count;

                idbgx(Debug::mpipc, this << ' ' << _rpoolid << " new connection " << conuid << " - active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);
            } else {
                idbgx(Debug::mpipc, this << ' ' << conuid << " " << error.message());
            }
        } else {
            //enough pending connection - cache the addrvec for later use
            rpool.connect_addr_vec = std::move(presolvemsg->addrvec);
            rpool.connect_addr_vec.pop_back();
        }
    } else {
        unique_lock<std::mutex> lock(d.poolMutex(_rpoolid.index));

        doTryCreateNewConnectionForPool(_rpoolid.index, error);
    }
}
//-----------------------------------------------------------------------------
void Service::doFetchResendableMessagesFromConnection(
    Connection& _rcon)
{

    //the final front message in msgorder_inner_list should be the oldest one from connection
    _rcon.forEveryMessagesNewerToOlder(
        [this](
            const ConnectionPoolId& _rpool_id,
            MessageBundle&          _rmsgbundle,
            MessageId const&        _rmsgid) {
            this->doPushFrontMessageToPool(_rpool_id, _rmsgbundle, _rmsgid);
        });
}
//-----------------------------------------------------------------------------
void Service::doPushFrontMessageToPool(
    const ConnectionPoolId& _rpool_id,
    MessageBundle&          _rmsgbundle,
    MessageId const&        _rmsgid)
{

    vdbgx(Debug::mpipc, this);

    ConnectionPoolStub& rpool(d.pooldq[_rpool_id.index]);

    SOLID_ASSERT(rpool.unique == _rpool_id.unique);

    if (
        Message::is_idempotent(_rmsgbundle.message_flags) or (not Message::is_started_send(_rmsgbundle.message_flags) and not Message::is_done_send(_rmsgbundle.message_flags))) {

        vdbgx(Debug::mpipc, this << " " << _rmsgbundle.message_ptr.get());

        _rmsgbundle.message_flags &= ~(MessageFlags::DoneSend | MessageFlags::StartedSend);

        if (_rmsgid.isInvalid()) {
            rpool.pushFrontMessage(
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags);
        } else {
            rpool.reinsertFrontMessage(
                _rmsgid,
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags);
        }
    }
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::activateConnection(Connection& _rconnection, ObjectIdT const& _robjuid)
{

    vdbgx(Debug::mpipc, this);

    const size_t            pool_index = _rconnection.poolId().index;
    unique_lock<std::mutex> lock2(d.poolMutex(pool_index));
    ConnectionPoolStub&     rpool(d.pooldq[pool_index]);
    ErrorConditionT         error;

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_unknown_pool;
        return error;
    }

    if (
        rpool.isMainConnectionStopping()) {
        manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());

        --rpool.pending_connection_count;
        ++rpool.active_connection_count;

        rpool.main_connection_id = _robjuid;
        rpool.resetMainConnectionStopping();
        rpool.setMainConnectionActive();

        return error;
    }

    const size_t new_active_connection_count = rpool.active_connection_count + 1;

    if (
        new_active_connection_count > configuration().pool_max_active_connection_count) {
        error = error_service_too_many_active_connections;
        return error;
    }

    --rpool.pending_connection_count;
    ++rpool.active_connection_count;
    {
        ErrorConditionT err;
        doTryCreateNewConnectionForPool(pool_index, err);
    }

    return error;
}
//-----------------------------------------------------------------------------
void Service::acceptIncomingConnection(SocketDevice& _rsd)
{

    vdbgx(Debug::mpipc, this);

    size_t                  pool_idx;
    unique_lock<std::mutex> lock(d.mtx);

    if (d.conpoolcachestk.size()) {
        pool_idx = d.conpoolcachestk.top();
        d.conpoolcachestk.pop();
    } else {
        pool_idx = this->doPushNewConnectionPool();
    }

    {
        unique_lock<std::mutex> lock2(d.poolMutex(pool_idx));

        ConnectionPoolStub& rpool(d.pooldq[pool_idx]);

        DynamicPointer<aio::Object> objptr(new_connection(configuration(), _rsd, ConnectionPoolId(pool_idx, rpool.unique), rpool.name));

        solid::ErrorConditionT error;

        ObjectIdT con_id = d.config.scheduler().startObject(
            objptr, *this, make_event(GenericEvents::Start), error);

        idbgx(Debug::mpipc, this << " receive connection [" << con_id << "] error = " << error.message());

        if (error) {
            SOLID_ASSERT(con_id.isInvalid());
            rpool.clear();
            d.conpoolcachestk.push(pool_idx);
        } else {
            SOLID_ASSERT(con_id.isValid());
            ++rpool.pending_connection_count;
            rpool.main_connection_id = con_id;
        }
    }
}
//-----------------------------------------------------------------------------
void Service::onIncomingConnectionStart(ConnectionContext& _rconctx)
{
    vdbgx(Debug::mpipc, this);
    configuration().server.connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onOutgoingConnectionStart(ConnectionContext& _rconctx)
{
    vdbgx(Debug::mpipc, this);
    configuration().client.connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onConnectionStop(ConnectionContext& _rconctx)
{
    vdbgx(Debug::mpipc, this);
    configuration().connection_stop_fnc(_rconctx);
}
//=============================================================================
//-----------------------------------------------------------------------------
/*virtual*/ Message::~Message()
{
}
//-----------------------------------------------------------------------------
//=============================================================================
struct ResolveF {

    ResolveCompleteFunctionT cbk;

    void operator()(ResolveData& _rrd, ErrorCodeT const& _rerror)
    {
        AddressVectorT addrvec;
        if (!_rerror) {
            for (auto it = _rrd.begin(); it != _rrd.end(); ++it) {
                addrvec.push_back(SocketAddressStub(it));
                idbgx(Debug::mpipc, "add resolved address: " << addrvec.back());
            }
        }
        std::reverse(addrvec.begin(), addrvec.end());
        cbk(std::move(addrvec));
    }
};

void InternetResolverF::operator()(const std::string& _name, ResolveCompleteFunctionT& _cbk)
{

    std::string tmp;
    const char* hst_name;
    const char* svc_name;

    size_t off = _name.rfind(':');
    if (off != std::string::npos) {
        tmp      = _name.substr(0, off);
        hst_name = tmp.c_str();
        svc_name = _name.c_str() + off + 1;
        if (!svc_name[0]) {
            svc_name = default_service.c_str();
        }
    } else {
        hst_name = _name.c_str();
        svc_name = default_service.c_str();
    }

    ResolveF fnc;

    fnc.cbk = std::move(_cbk);

    rresolver.requestResolve(fnc, hst_name, svc_name, 0, this->family, SocketInfo::Stream);
}
//=============================================================================

std::ostream& operator<<(std::ostream& _ros, RecipientId const& _con_id)
{
    _ros << '{' << _con_id.connectionId() << "}{" << _con_id.poolId() << '}';
    return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& _ros, RequestId const& _msg_id)
{
    _ros << '{' << _msg_id.index << ',' << _msg_id.unique << '}';
    return _ros;
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& _ros, MessageId const& _msg_id)
{
    _ros << '{' << _msg_id.index << ',' << _msg_id.unique << '}';
    return _ros;
}
//=============================================================================
} //namespace mpipc
} //namespace frame
} //namespace solid
