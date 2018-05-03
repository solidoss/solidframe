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
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/queue.hpp"

#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "solid/system/mutualstore.hpp"
#include "solid/utility/innerlist.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/string.hpp"

#include "mpipcconnection.hpp"
#include "mpipclistener.hpp"
#include "mpipcutility.hpp"

using namespace std;

namespace solid {
namespace frame {
namespace mpipc {

const LoggerT logger("solid::frame::mpipc");

//=============================================================================
using NameMapT       = std::unordered_map<const char*, size_t, CStringHash, CStringEqual>;
using ObjectIdQueueT = Queue<ObjectIdT>;

enum {
    InnerLinkOrder = 0,
    InnerLinkAsync,
    InnerLinkCount
};

//-----------------------------------------------------------------------------

struct MessageStub : inner::Node<InnerLinkCount> {

    enum {
        CancelableFlag = 1
    };

    MessageStub(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _msgflags,
        std::string&              _rmsg_url)
        : msgbundle(_rmsgptr, _msg_type_idx, _msgflags, _rcomplete_fnc, _rmsg_url)
        , unique(0)
        , flags(0)
    {
    }

    MessageStub(
        MessageStub&& _rmsg)
        : inner::Node<InnerLinkCount>(std::move(_rmsg))
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
using MessageOrderInnerListT = inner::List<MessageVectorT, InnerLinkOrder>;
using MessageCacheInnerListT = inner::List<MessageVectorT, InnerLinkOrder>;
using MessageAsyncInnerListT = inner::List<MessageVectorT, InnerLinkAsync>;

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
    uint16_t               persistent_connection_count;
    uint16_t               pending_connection_count;
    uint16_t               active_connection_count;
    uint16_t               stopping_connection_count;
    std::string            name; //because c_str() pointer is given to connection - name should allways be std::moved
    ObjectIdT              main_connection_id;
    MessageVectorT         msgvec;
    MessageOrderInnerListT msgorder_inner_list;
    MessageCacheInnerListT msgcache_inner_list;
    MessageAsyncInnerListT msgasync_inner_list;
    ObjectIdQueueT         conn_waitingq;
    uint8_t                flags;
    uint8_t                retry_connect_count;
    AddressVectorT         connect_addr_vec;

    ConnectionPoolStub()
        : unique(0)
        , persistent_connection_count(0)
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
        , persistent_connection_count(_rpool.persistent_connection_count)
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
        persistent_connection_count = 0;
        pending_connection_count    = 0;
        active_connection_count     = 0;
        stopping_connection_count   = 0;
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
        const MessageFlagsT&      _flags,
        std::string&              _msg_url)
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

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, _msg_url);

        //SOLID_ASSERT(rmsgstub.msgbundle.message_ptr.get());

        return MessageId(idx, rmsgstub.unique);
    }

    MessageId pushBackMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url);

        msgorder_inner_list.pushBack(msgid.index);

        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushBack(msgid.index);
            solid_dbg(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        SOLID_ASSERT(msgorder_inner_list.check());

        return msgid;
    }

    MessageId pushFrontMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url);

        msgorder_inner_list.pushFront(msgid.index);

        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(msgid.index);
            solid_dbg(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        SOLID_ASSERT(msgorder_inner_list.check());

        return msgid;
    }

    MessageId reinsertFrontMessage(
        MessageId const&          _rmsgid,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url)
    {
        MessageStub& rmsgstub(msgvec[_rmsgid.index]);

        SOLID_ASSERT(!rmsgstub.msgbundle.message_ptr && rmsgstub.unique == _rmsgid.unique);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, _msg_url);

        msgorder_inner_list.pushFront(_rmsgid.index);

        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(_rmsgid.index);
            solid_dbg(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
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
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgcache_inner_list.pushBack(msgorder_inner_list.popFront());

        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void popFrontMessage()
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.popFront();

        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void eraseMessageOrder(const size_t _msg_idx)
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void eraseMessageOrderAsync(const size_t _msg_idx)
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        const MessageStub& rmsgstub(msgvec[_msg_idx]);
        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
            SOLID_ASSERT(msgasync_inner_list.contains(_msg_idx));
            msgasync_inner_list.erase(_msg_idx);
        }
        SOLID_ASSERT(msgorder_inner_list.check());
    }

    void clearPopAndCacheMessage(const size_t _msg_idx)
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
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
        if (_rmsgid.index < msgvec.size() && msgvec[_rmsgid.index].unique == _rmsgid.unique) {
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
        return pending_connection_count == 0 && active_connection_count == 0 && stopping_connection_count == 0;
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
            return !msgorder_inner_list.front().msgbundle.message_ptr;
        }
    }

    bool hasAnyMessage() const
    {
        return !hasNoMessage();
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
        return msgcache_inner_list.empty() && msgvec.size() >= _max_message_queue_size;
    }

    bool shouldClose() const
    {
        return isClosing() && hasNoMessage();
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
    std::string          tmp_str;
};
//=============================================================================

Service::Service(
    UseServiceShell _force_shell)
    : BaseT(_force_shell)
    , impl_(make_pimpl<Data>(*this))
{
}

//! Destructor
Service::~Service()
{
    stop(true);
}
//-----------------------------------------------------------------------------
Configuration const& Service::configuration() const
{
    return impl_->config;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::start()
{
    lock_guard<std::mutex> lock(impl_->mtx);
    ErrorConditionT        err = doStart();
    return err;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doStart()
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;

    if (!BaseT::start()) {
        error = error_service_start;
        return error;
    }

    if (configuration().server.listener_address_str.size()) {
        std::string tmp;
        const char* hst_name;
        const char* svc_name;

        size_t off = impl_->config.server.listener_address_str.rfind(':');
        if (off != std::string::npos) {
            tmp      = impl_->config.server.listener_address_str.substr(0, off);
            hst_name = tmp.c_str();
            svc_name = impl_->config.server.listener_address_str.c_str() + off + 1;
            if (!svc_name[0]) {
                svc_name = impl_->config.server.listener_service_str.c_str();
            }
        } else {
            hst_name = impl_->config.server.listener_address_str.c_str();
            svc_name = impl_->config.server.listener_service_str.c_str();
        }

        ResolveData  rd = synchronous_resolve(hst_name, svc_name, 0, -1, SocketInfo::Stream);
        SocketDevice sd;

        if (!rd.empty()) {
            sd.create(rd.begin());
            const ErrorCodeT errc = sd.prepareAccept(rd.begin(), Listener::backlog_size());
            if (errc) {
                sd.close();
            }
        }

        if (sd) {

            SocketAddress local_address;

            sd.localAddress(local_address);

            impl_->config.server.listener_port = local_address.port();

            DynamicPointer<aio::Object> objptr(new Listener(sd));

            ObjectIdT conuid = impl_->config.scheduler().startObject(objptr, *this, make_event(GenericEvents::Start), error);
            (void)conuid;
            if (error) {
                return error;
            }
        } else {
            error = error_service_start_listener;
            return error;
        }
    }

    if (impl_->config.pools_mutex_count > impl_->mtxsarrcp) {
        delete[] impl_->pmtxarr; //TODO: delete
        impl_->pmtxarr   = new std::mutex[impl_->config.pools_mutex_count];
        impl_->mtxsarrcp = impl_->config.pools_mutex_count;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::reconfigure(Configuration&& _ucfg)
{

    solid_dbg(logger, Verbose, this);

    BaseT::stop(true); //block until all objects are destroyed

    lock_guard<std::mutex> lock(impl_->mtx);

    {
        ErrorConditionT error;

        error = _ucfg.check();

        if (error)
            return error;

        impl_->config.reset(std::move(_ucfg));
    }
    return doStart();
}
//-----------------------------------------------------------------------------
size_t Service::doPushNewConnectionPool()
{

    solid_dbg(logger, Verbose, this);

    impl_->lockAllConnectionPoolMutexes();

    for (size_t i = 0; i < impl_->mtxsarrcp; ++i) {
        const size_t pool_idx = impl_->pooldq.size();
        impl_->pooldq.push_back(ConnectionPoolStub());
        impl_->conpoolcachestk.push(pool_idx);
    }
    impl_->unlockAllConnectionPoolMutexes();
    size_t idx = impl_->conpoolcachestk.top();
    impl_->conpoolcachestk.pop();
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
        solid_dbg(logger, Info, "OnResolveF(addrvec of size " << _raddrvec.size() << ")");
        event.any() = ResolveMessage(std::move(_raddrvec));
        rm.notify(objuid, std::move(event));
    }
};

//-----------------------------------------------------------------------------

ErrorConditionT Service::createConnectionPool(const char* _recipient_url, const size_t _persistent_connection_count /* = 1*/)
{
    solid::ErrorConditionT error;
    size_t                 pool_index;
    std::string            message_url;

    lock_guard<std::mutex> lock(impl_->mtx);
    const char*            recipient_name = configuration().extract_recipient_name_fnc(_recipient_url, message_url, impl_->tmp_str);

    if (recipient_name == nullptr || recipient_name[0] == '\0') {
        solid_dbg(logger, Error, this << " failed extracting recipient name");
        error = error_service_invalid_url;
        return error;
    }

    {
        NameMapT::const_iterator it = impl_->namemap.find(recipient_name);

        if (it != impl_->namemap.end()) {
            pool_index = it->second;

            lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
            ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

            rpool.persistent_connection_count = static_cast<uint16_t>(_persistent_connection_count);
            doTryCreateNewConnectionForPool(pool_index, error);
        } else {
            if (impl_->conpoolcachestk.size()) {
                pool_index = impl_->conpoolcachestk.top();
                impl_->conpoolcachestk.pop();
            } else {
                pool_index = this->doPushNewConnectionPool();
            }

            lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
            ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

            rpool.name                        = recipient_name;
            rpool.persistent_connection_count = static_cast<uint16_t>(_persistent_connection_count);

            if (!doTryCreateNewConnectionForPool(pool_index, error)) {
                rpool.clear();
                impl_->conpoolcachestk.push(pool_index);
                return error;
            }

            impl_->namemap[rpool.name.c_str()] = pool_index;
        }
    }

    return error;
}

//-----------------------------------------------------------------------------
ErrorConditionT Service::doSendMessage(
    const char*               _recipient_url,
    const RecipientId&        _rrecipient_id_in,
    MessagePointerT&          _rmsgptr,
    MessageCompleteFunctionT& _rcomplete_fnc,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    const MessageFlagsT&      _flags)
{

    solid_dbg(logger, Verbose, this);

    solid::ErrorConditionT error;
    size_t                 pool_index;
    uint32_t               unique    = -1;
    bool                   check_uid = false;
    lock_guard<std::mutex> lock(impl_->mtx);

    if (!isRunning()) {
        solid_dbg(logger, Error, this << " service stopping");
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
        solid_dbg(logger, Error, this << " message type not registered");
        error = error_service_message_unknown_type;
        return error;
    }

    std::string message_url;
    const char* recipient_name = configuration().extract_recipient_name_fnc(_recipient_url, message_url, impl_->tmp_str);

    if (_recipient_url != nullptr && (recipient_name == nullptr || recipient_name[0] == '\0')) {
        solid_dbg(logger, Error, this << " failed extracting recipient name");
        error = error_service_invalid_url;
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
            _flags,
            message_url);
    } else if (recipient_name) {
        NameMapT::const_iterator it = impl_->namemap.find(recipient_name);

        if (it != impl_->namemap.end()) {
            pool_index = it->second;
        } else {
            if (configuration().isServerOnly()) {
                solid_dbg(logger, Error, this << " request for name resolve for a server only configuration");
                error = error_service_server_only;
                return error;
            }

            return this->doSendMessageToNewPool(
                recipient_name, _rmsgptr, msg_type_idx,
                _rcomplete_fnc, _precipient_id_out, _pmsgid_out, _flags, message_url);
        }
    } else if (
        static_cast<size_t>(_rrecipient_id_in.poolid.index) < impl_->pooldq.size()) {
        //we cannot check the uid right now because we need a lock on the pool's mutex
        check_uid  = true;
        pool_index = static_cast<size_t>(_rrecipient_id_in.poolid.index);
        unique     = _rrecipient_id_in.poolid.unique;
    } else {
        solid_dbg(logger, Error, this << " recipient does not exist");
        error = error_service_unknown_recipient;
        return error;
    }

    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (check_uid && rpool.unique != unique) {
        //failed uid check
        solid_dbg(logger, Error, this << " connection pool does not exist");
        error = error_service_unknown_pool;
        return error;
    }

    if (rpool.isClosing()) {
        solid_dbg(logger, Error, this << " connection pool is stopping");
        error = error_service_pool_stopping;
        return error;
    }

    if (rpool.isFull(configuration().pool_max_message_queue_size)) {
        solid_dbg(logger, Error, this << " connection pool is full");
        error = error_service_pool_full;
        return error;
    }

    if (_precipient_id_out) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_index, rpool.unique);
    }

    //At this point we can fetch the message from user's pointer
    //because from now on we can call complete on the message
    const MessageId msgid = rpool.pushBackMessage(_rmsgptr, msg_type_idx, _rcomplete_fnc, _flags, message_url);

    if (_pmsgid_out) {

        MessageStub& rmsgstub(rpool.msgvec[msgid.index]);

        rmsgstub.makeCancelable();

        *_pmsgid_out = msgid;
        solid_dbg(logger, Info, this << " set message id to " << *_pmsgid_out);
    }

    bool success = false;

    if (
        rpool.isCleaningOneShotMessages() && Message::is_one_shot(_flags)) {
        success = manager().notify(
            rpool.main_connection_id,
            Connection::eventClosePoolMessage(msgid));

        if (success) {
            solid_dbg(logger, Verbose, this << " message " << msgid << " from pool " << pool_index << " sent for canceling to " << rpool.main_connection_id);
            //erase/unlink the message from any list
            if (rpool.msgorder_inner_list.contains(msgid.index)) {
                rpool.eraseMessageOrderAsync(msgid.index);
            }
        } else {
            SOLID_THROW("Message Cancel connection not available");
        }
    }

    if (
        !success && Message::is_synchronous(_flags) &&
        //      rpool.main_connection_id.isValid() and
        rpool.isMainConnectionActive()) {
        success = manager().notify(
            rpool.main_connection_id,
            Connection::eventNewMessage());
        SOLID_ASSERT(success);
    }

    if (!success && !Message::is_synchronous(_flags)) {
        success = doTryNotifyPoolWaitingConnection(pool_index);
    }

    if (!success) {
        doTryCreateNewConnectionForPool(pool_index, error);
        error.clear();
    }

    if (!success) {
        solid_dbg(logger, Warning, this << " no connection notified about the new message");
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
    const MessageFlagsT&      _flags,
    std::string&              _msg_url)
{
    //d.mtx must be locked

    solid_dbg(logger, Verbose, this);

    if (!_rrecipient_id_in.isValidPool()) {
        SOLID_ASSERT(false);
        return error_service_unknown_connection;
    }

    const size_t pool_index = static_cast<size_t>(_rrecipient_id_in.poolId().index);

    if (pool_index >= impl_->pooldq.size() || impl_->pooldq[pool_index].unique != _rrecipient_id_in.poolId().unique) {
        return error_service_unknown_connection;
    }

    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool = impl_->pooldq[pool_index];
    solid::ErrorConditionT error;
    const bool             is_server_side_pool = rpool.isServerSide(); //unnamed pool has a single connection

    MessageId msgid;

    bool success = false;

    if (is_server_side_pool) {
        //for a server pool we want to enque messages in the pool
        //
        msgid   = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags | MessageFlagsE::OneShotSend, _msg_url);
        success = manager().notify(
            _rrecipient_id_in.connectionId(),
            Connection::eventNewMessage());
    } else {
        msgid   = rpool.insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags | MessageFlagsE::OneShotSend, _msg_url);
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
    const MessageFlagsT&      _flags,
    std::string&              _msg_url)
{

    solid_dbg(logger, Verbose, this);

    solid::ErrorConditionT error;
    size_t                 pool_index;

    if (impl_->conpoolcachestk.size()) {
        pool_index = impl_->conpoolcachestk.top();
        impl_->conpoolcachestk.pop();
    } else {
        pool_index = this->doPushNewConnectionPool();
    }

    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    rpool.name = _recipient_name;

    MessageId msgid = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url);

    if (!doTryCreateNewConnectionForPool(pool_index, error)) {
        solid_dbg(logger, Error, this << " Starting Session: " << error.message());
        rpool.popFrontMessage();
        rpool.clear();
        impl_->conpoolcachestk.push(pool_index);
        return error;
    }

    impl_->namemap[rpool.name.c_str()] = pool_index;

    if (_precipient_id_out) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_index, rpool.unique);
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
    const size_t     _pool_index,
    const size_t     _msg_idx)
{

    solid_dbg(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    MessageStub&        rmsgstub                = rpool.msgvec[_msg_idx];
    const bool          message_is_asynchronous = Message::is_asynchronous(rmsgstub.msgbundle.message_flags);
    const bool          message_is_null         = !rmsgstub.msgbundle.message_ptr;
    bool                success                 = false;

    SOLID_ASSERT(!Message::is_canceled(rmsgstub.msgbundle.message_flags));

    if (!rmsgstub.msgbundle.message_ptr) {
        return false;
    }

    if (rmsgstub.isCancelable()) {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId(_msg_idx, rmsgstub.unique));

        if (success && !message_is_null) {

            rmsgstub.objid = _robjuid;

            rpool.eraseMessageOrder(_msg_idx);

            if (message_is_asynchronous) {
                rpool.msgasync_inner_list.erase(_msg_idx);
            }
        }

    } else {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId());

        if (success && !message_is_null) {

            rpool.eraseMessageOrder(_msg_idx);

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
    const size_t     _pool_index,
    const MessageId& _rmsg_id)
{

    solid_dbg(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);

    MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
    bool         success  = false;

    SOLID_ASSERT(!Message::is_canceled(rmsgstub.msgbundle.message_flags));

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

    solid_dbg(logger, Verbose, this << " " << &_rconnection);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    ErrorConditionT error;

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_unknown_pool;
        return error;
    }

    if (rpool.isMainConnectionStopping() && rpool.main_connection_id != _robjuid) {

        solid_dbg(logger, Info, this << ' ' << &_rconnection << " switch message main connection from " << rpool.main_connection_id << " to " << _robjuid);

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
        solid_dbg(logger, Info, this << ' ' << &_rconnection << " pool is FastClosing");
        error = error_service_pool_stopping;
        return error;
    }

    if (_rconnection.isWriterEmpty() && rpool.shouldClose()) {
        solid_dbg(logger, Info, this << ' ' << &_rconnection << " pool is DelayClosing");
        error = error_service_pool_stopping;
        return error;
    }

    solid_dbg(logger, Info, this << ' ' << &_rconnection << " messages in pool: " << rpool.msgorder_inner_list);

    bool       connection_may_handle_more_messages        = !_rconnection.isFull(configuration());
    const bool connection_can_handle_synchronous_messages = _robjuid == rpool.main_connection_id;

    //We need to push as many messages as we can to the connection
    //in order to handle eficiently the situation with multiple small messages.

    if (_rconnection.pendingMessageVector().size()) {
        //connection has pending messages
        //fetch as many as we can
        size_t count = 0;

        for (const auto& rmsgid : _rconnection.pendingMessageVector()) {
            if (rmsgid.index < rpool.msgvec.size() && rmsgid.unique == rpool.msgvec[rmsgid.index].unique) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(_rconnection, _robjuid, pool_index, rmsgid);
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
        while (rpool.msgorder_inner_list.size() && connection_may_handle_more_messages) {
            connection_may_handle_more_messages = doTryPushMessageToConnection(
                _rconnection,
                _robjuid,
                pool_index,
                rpool.msgorder_inner_list.frontIndex());
        }

    } else {

        //use the async inner queue
        while (rpool.msgasync_inner_list.size() && connection_may_handle_more_messages) {
            connection_may_handle_more_messages = doTryPushMessageToConnection(
                _rconnection,
                _robjuid,
                pool_index,
                rpool.msgasync_inner_list.frontIndex());
        }
    }
    //a connection will either be in conn_waitingq
    //or it will call pollPoolForUpdates asap.
    //this is because we need to be able to notify connection about
    //pool force close imeditely
    if (!_rconnection.isInPoolWaitingQueue()) {
        rpool.conn_waitingq.push(_robjuid);
        _rconnection.setInPoolWaitingQueue();
    }

    return error;
}
//-----------------------------------------------------------------------------
void Service::rejectNewPoolMessage(Connection const& _rconnection)
{

    solid_dbg(logger, Verbose, this);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique)
        return;

    doTryNotifyPoolWaitingConnection(pool_index);
}
//-----------------------------------------------------------------------------
bool Service::doTryNotifyPoolWaitingConnection(const size_t _pool_index)
{

    solid_dbg(logger, Verbose, this);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    bool                success = false;

    //we were not able to handle the message, try notify another connection
    while (!success && rpool.conn_waitingq.size()) {
        //a connection is waiting for something to send
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        success = manager().notify(
            objuid,
            Connection::eventNewQueueMessage());
    }
    return success;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doDelayCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock(impl_->mtx);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();
    solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");

    if (rpool.name.size()) {
        impl_->namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;
    std::string     empty_str;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, 0, empty_str);
    (void)msgid;

    //notify all waiting connections about the new message
    while (rpool.conn_waitingq.size()) {
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            objuid,
            Connection::eventNewQueueMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doForceCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock(impl_->mtx);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (pool_index >= impl_->pooldq.size() || rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();
    rpool.setFastClosing();

    solid_dbg(logger, Verbose, this << " pool " << pool_index << " set fast closing");

    if (rpool.name.size()) {
        impl_->namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;
    string          empty_str;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, {MessageFlagsE::Synchronous}, empty_str);
    (void)msgid;

    //no reason to cancel all messages - they'll be handled on connection stop.
    //notify all waiting connections about the new message
    while (rpool.conn_waitingq.size()) {
        ObjectIdT objuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            objuid,
            Connection::eventNewQueueMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::cancelMessage(RecipientId const& _rrecipient_id, MessageId const& _rmsg_id)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    if (_rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
        bool         success  = false;

        if (Message::is_canceled(rmsgstub.msgbundle.message_flags)) {
            error = error_service_message_already_canceled;
        } else {

            if (rmsgstub.objid.isValid()) { //message handled by a connection

                solid_dbg(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " is handled by connection " << rmsgstub.objid);

                SOLID_ASSERT(!rmsgstub.msgbundle.message_ptr);

                rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);

                success = manager().notify(
                    rmsgstub.objid,
                    Connection::eventCancelConnMessage(rmsgstub.msgid));

                if (success) {
                    rmsgstub.clear();
                    rpool.msgcache_inner_list.pushBack(_rmsg_id.index);
                } else {
                    rmsgstub.msgid = MessageId();
                    rmsgstub.objid = ObjectIdT();
                    error          = error_service_message_lost;
                    SOLID_THROW("Lost message");
                }
            }

            if (rmsgstub.msgbundle.message_ptr) {

                solid_dbg(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " not handled by any connection");

                rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);

                success = manager().notify(
                    rpool.main_connection_id,
                    Connection::eventCancelPoolMessage(_rmsg_id));

                if (success) {
                    solid_dbg(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " sent for canceling to " << rpool.main_connection_id);
                    //erase/unlink the message from any list
                    if (rpool.msgorder_inner_list.contains(_rmsg_id.index)) {
                        rpool.eraseMessageOrderAsync(_rmsg_id.index);
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

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity));

    if (!success) {
        solid_dbg(logger, Warning, this << " failed notify enter active event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyStartSecureHandshake(
    RecipientId const&                          _rrecipient_id,
    ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventStartSecure(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_dbg(logger, Warning, this << " failed notify start secure event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyEnterPassiveState(
    RecipientId const&                        _rrecipient_id,
    ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterPassive(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_dbg(logger, Warning, this << " failed notify enter passive event to " << _rrecipient_id.connectionId());
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

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventSendRaw(std::move(_ucomplete_fnc), std::move(_rdata)));

    if (!success) {
        solid_dbg(logger, Warning, this << " failed notify send raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyRecvRawData(
    RecipientId const&                       _rrecipient_id,
    ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
{

    solid_dbg(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventRecvRaw(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_dbg(logger, Warning, this << " failed notify recv raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
bool Service::fetchMessage(Connection& _rcon, ObjectIdT const& _robjuid, MessageId const& _rmsg_id)
{

    solid_dbg(logger, Verbose, this);

    lock_guard<std::mutex> lock2(impl_->poolMutex(static_cast<size_t>(_rcon.poolId().index)));
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        return doTryPushMessageToConnection(_rcon, _robjuid, pool_index, _rmsg_id.index);
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::fetchCanceledMessage(Connection const& _rcon, MessageId const& _rmsg_id, MessageBundle& _rmsg_bundle)
{

    solid_dbg(logger, Verbose, this);

    lock_guard<std::mutex> lock2(impl_->poolMutex(static_cast<size_t>(_rcon.poolId().index)));
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];

        if (rpool.msgorder_inner_list.contains(_rmsg_id.index)) {
            rpool.eraseMessageOrderAsync(_rmsg_id.index);
        }

        //rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);
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

    solid_dbg(logger, Verbose, this);

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

    solid_dbg(logger, Verbose, this);
    lock_guard<std::mutex> lock(impl_->mtx);
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    _rseconds_to_wait = 0;
    _rmsg_bundle.clear();

    SOLID_ASSERT(rpool.unique == _rcon.poolId().unique);
    if (rpool.unique != _rcon.poolId().unique)
        return false;

    solid_dbg(logger, Info, this << ' ' << pool_index << " active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);

    if (!rpool.isMainConnection(_robjuid)) {
        return doNonMainConnectionStopping(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (!rpool.isLastConnection()) {
        return doMainConnectionStoppingNotLast(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isCleaningOneShotMessages()) {
        return doMainConnectionStoppingCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isCleaningAllMessages()) {
        return doMainConnectionStoppingCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (rpool.isRestarting() && isRunning()) {
        return doMainConnectionRestarting(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else if (!rpool.isFastClosing() && !rpool.isServerSide() && isRunning() && _rerror != error_connection_resolve) {
        return doMainConnectionStoppingPrepareCleanOneShot(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    } else {
        return doMainConnectionStoppingPrepareCleanAll(_rcon, _robjuid, _rseconds_to_wait, _rmsg_id, _rmsg_bundle, _revent_context, _rerror);
    }
}
//-----------------------------------------------------------------------------
bool Service::doNonMainConnectionStopping(
    Connection& _rcon, ObjectIdT const& _robjuid,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle& /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        SOLID_ASSERT(!_rcon.isServer());
        SOLID_ASSERT(rpool.pending_connection_count >= 0);
        --rpool.pending_connection_count;
    }

    ++rpool.stopping_connection_count;

    if (rpool.isLastConnection() && rpool.isMainConnectionStopping()) {
        manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());
    }

    if (!rpool.isFastClosing()) {
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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    //is main connection but is not the last one
    _rseconds_to_wait = 10;
    rpool.setMainConnectionStopping();
    rpool.resetMainConnectionActive();

    if (!rpool.isFastClosing()) {
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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    size_t* pmsgidx = _revent_context.any().cast<size_t>();
    SOLID_ASSERT(pmsgidx);
    const size_t crtmsgidx = *pmsgidx;

    if (crtmsgidx != InvalidIndex()) {

        MessageStub& rmsgstub = rpool.msgvec[crtmsgidx];

        *pmsgidx = rpool.msgorder_inner_list.nextIndex(crtmsgidx);

        if (rpool.msgorder_inner_list.size() != 1) {
            SOLID_ASSERT(rpool.msgorder_inner_list.contains(crtmsgidx));
        } else if (rpool.msgorder_inner_list.size() == 1) {
            SOLID_ASSERT(rpool.msgorder_inner_list.frontIndex() == crtmsgidx);
        } else {
            SOLID_ASSERT(false);
        }

        if (rmsgstub.msgbundle.message_ptr.get() && Message::is_one_shot(rmsgstub.msgbundle.message_flags)) {
            _rmsg_bundle = std::move(rmsgstub.msgbundle);
            _rmsg_id     = MessageId(crtmsgidx, rmsgstub.unique);
            rpool.clearPopAndCacheMessage(crtmsgidx);
        } else if (!rmsgstub.msgbundle.message_ptr && rpool.msgorder_inner_list.size() == 1) {
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
    ulong& /*_rseconds_to_wait*/,
    MessageId&     _rmsg_id,
    MessageBundle& _rmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

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

        if (rpool.name.size() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
            impl_->namemap.erase(rpool.name.c_str());
            rpool.setClosing();
            solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");
        }

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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    //the last connection
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    doFetchResendableMessagesFromConnection(_rcon);

    rpool.resetMainConnectionActive();

    if (rpool.msgorder_inner_list.size() || rpool.persistent_connection_count != 0) {
        rpool.setCleaningOneShotMessages();

        _revent_context.any() = rpool.msgorder_inner_list.frontIndex();

        return false;
    } else {
        if (_rcon.isActiveState()) {
            --rpool.active_connection_count;
        } else {
            SOLID_ASSERT(rpool.pending_connection_count >= 0);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;

        if (rpool.name.size() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
            impl_->namemap.erase(rpool.name.c_str());
            rpool.setClosing();
            solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");
        }
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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    //the last connection - fast closing or server side
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    if (rpool.name.size() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
        impl_->namemap.erase(rpool.name.c_str());
    }

    rpool.setCleaningAllMessages();
    rpool.resetMainConnectionActive();
    rpool.setFastClosing();
    rpool.setClosing();
    solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");

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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

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

    if (rpool.hasAnyMessage() || rpool.persistent_connection_count != 0) {

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

        if (!success) {
            if (_rcon.isActiveState()) {
                ++rpool.active_connection_count;
            } else {
                SOLID_ASSERT(!_rcon.isServer());
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

    solid_dbg(logger, Info, this << ' ' << &_rcon);

    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    lock_guard<std::mutex> lock(impl_->mtx);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    SOLID_ASSERT(rpool.unique == _rcon.poolId().unique);
    if (rpool.unique != _rcon.poolId().unique)
        return;

    --rpool.stopping_connection_count;
    rpool.resetRestarting();

    if (rpool.hasNoConnection()) {

        SOLID_ASSERT(rpool.msgorder_inner_list.empty());
        impl_->conpoolcachestk.push(pool_index);

        if (rpool.name.size() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
            impl_->namemap.erase(rpool.name.c_str());
        }

        rpool.clear();
    }
}
//-----------------------------------------------------------------------------
bool Service::doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT& _rerror)
{

    solid_dbg(logger, Verbose, this);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    const bool          is_new_connection_needed = rpool.active_connection_count < rpool.persistent_connection_count || (rpool.hasAnyMessage() && rpool.conn_waitingq.size() < rpool.msgorder_inner_list.size());

    if (
        rpool.active_connection_count < configuration().pool_max_active_connection_count && rpool.pending_connection_count == 0 && is_new_connection_needed && isRunning()) {

        solid_dbg(logger, Info, this << " try create new connection in pool " << rpool.active_connection_count << " pending connections " << rpool.pending_connection_count);

        DynamicPointer<aio::Object> objptr(new_connection(configuration(), ConnectionPoolId(_pool_index, rpool.unique), rpool.name));

        ObjectIdT conuid = impl_->config.scheduler().startObject(objptr, *this, make_event(GenericEvents::Start), _rerror);

        if (!_rerror) {

            solid_dbg(logger, Info, this << " Success starting Connection Pool object: " << conuid.index << ',' << conuid.unique);

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

                event.any() = ResolveMessage(std::move(rpool.connect_addr_vec));

                manager().notify(conuid, std::move(event));
            }

            return true;
        }
    } else {
        _rerror = error_service_connection_not_needed;
    }
    return false;
}
//-----------------------------------------------------------------------------
void Service::forwardResolveMessage(ConnectionPoolId const& _rpoolid, Event& _revent)
{

    solid_dbg(logger, Verbose, this);

    ResolveMessage* presolvemsg = _revent.any().cast<ResolveMessage>();
    ErrorConditionT error;

    if (presolvemsg->addrvec.size()) {
        lock_guard<std::mutex> lock(impl_->poolMutex(static_cast<size_t>(_rpoolid.index)));
        ConnectionPoolStub&    rpool(impl_->pooldq[static_cast<size_t>(_rpoolid.index)]);

        if (rpool.pending_connection_count < configuration().pool_max_pending_connection_count) {

            DynamicPointer<aio::Object> objptr(new_connection(configuration(), _rpoolid, rpool.name));

            ObjectIdT conuid = impl_->config.scheduler().startObject(objptr, *this, std::move(_revent), error);

            if (!error) {
                ++rpool.pending_connection_count;

                solid_dbg(logger, Info, this << ' ' << _rpoolid << " new connection " << conuid << " - active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);
            } else {
                solid_dbg(logger, Info, this << ' ' << conuid << " " << error.message());
            }
        } else {
            //enough pending connection - cache the addrvec for later use
            rpool.connect_addr_vec = std::move(presolvemsg->addrvec);
        }
    } else {
        lock_guard<std::mutex> lock(impl_->poolMutex(static_cast<size_t>(_rpoolid.index)));

        doTryCreateNewConnectionForPool(static_cast<size_t>(_rpoolid.index), error);
    }
}
//-----------------------------------------------------------------------------
void Service::doFetchResendableMessagesFromConnection(
    Connection& _rcon)
{

    solid_dbg(logger, Verbose, this << " " << &_rcon);
    //the final front message in msgorder_inner_list should be the oldest one from connection
    _rcon.fetchResendableMessages(*this,
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

    ConnectionPoolStub& rpool(impl_->pooldq[static_cast<size_t>(_rpool_id.index)]);

    solid_dbg(logger, Verbose, this << " " << _rmsgbundle.message_ptr.get() << " msgorder list sz = " << rpool.msgorder_inner_list.size());

    SOLID_ASSERT(rpool.unique == _rpool_id.unique);

    if (
        Message::is_idempotent(_rmsgbundle.message_flags) || !Message::is_done_send(_rmsgbundle.message_flags)) {

        solid_dbg(logger, Verbose, this << " " << _rmsgbundle.message_ptr.get());

        _rmsgbundle.message_flags.reset(MessageFlagsE::DoneSend).reset(MessageFlagsE::StartedSend);

        if (_rmsgid.isInvalid()) {
            rpool.pushFrontMessage(
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags,
                _rmsgbundle.message_url);
        } else {
            rpool.reinsertFrontMessage(
                _rmsgid,
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags,
                _rmsgbundle.message_url);
        }
    }
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::activateConnection(Connection& _rconnection, ObjectIdT const& _robjuid)
{

    solid_dbg(logger, Verbose, this);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);
    ErrorConditionT        error;

    SOLID_ASSERT(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_unknown_pool;
        return error;
    }

    if (_rconnection.isActiveState()) {
        error = error_service_already_active;
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

    solid_dbg(logger, Verbose, this);

    configuration().server.socket_device_setup_fnc(_rsd);

    size_t                 pool_index;
    lock_guard<std::mutex> lock(impl_->mtx);
    bool                   from_cache = impl_->conpoolcachestk.size() != 0;

    if (from_cache) {
        pool_index = impl_->conpoolcachestk.top();
        impl_->conpoolcachestk.pop();
    } else {
        pool_index = this->doPushNewConnectionPool();
    }

    {
        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));

        ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

        DynamicPointer<aio::Object> objptr(new_connection(configuration(), _rsd, ConnectionPoolId(pool_index, rpool.unique), rpool.name));

        solid::ErrorConditionT error;

        ObjectIdT con_id = impl_->config.scheduler().startObject(
            objptr, *this, make_event(GenericEvents::Start), error);

        solid_dbg(logger, Info, this << " receive connection [" << con_id << "] error = " << error.message());

        if (error) {
            SOLID_ASSERT(con_id.isInvalid());
            rpool.clear();
            impl_->conpoolcachestk.push(pool_index);
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
    solid_dbg(logger, Verbose, this);
    configuration().server.connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onOutgoingConnectionStart(ConnectionContext& _rconctx)
{
    solid_dbg(logger, Verbose, this);
    configuration().client.connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onConnectionStop(ConnectionContext& _rconctx)
{
    solid_dbg(logger, Verbose, this);
    configuration().relayEngine().stopConnection(_rconctx.relayId());
    configuration().connection_stop_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::sendRelay(const ObjectIdT& _rconid, RelayData&& _urelmsg)
{
    ErrorConditionT error;

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::sendRelayCancel(RelayData&& _urelmsg)
{
    ErrorConditionT error;
    return error;
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
                solid_dbg(logger, Info, "add resolved address: " << addrvec.back());
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
