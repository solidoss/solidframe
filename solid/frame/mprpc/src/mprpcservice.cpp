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

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/system/mutualstore.hpp"
#include "solid/utility/innerlist.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/string.hpp"

#include "mprpcconnection.hpp"
#include "mprpclistener.hpp"
#include "mprpcutility.hpp"

using namespace std;

namespace solid {
namespace frame {
namespace mprpc {
namespace {

const LoggerT logger("solid::frame::mprpc::service");

enum class PoolEvents {
    ConnectionStart,
    ConnectionActivate,
    ConnectionStop,
    PoolDisconnect,
    PoolStop,
};

const EventCategory<PoolEvents> pool_event_category{
    "solid::frame::mprpc::pool_event_category",
    [](const PoolEvents _evt) {
        switch (_evt) {
        case PoolEvents::ConnectionStart:
            return "ConnectionStart";
        case PoolEvents::ConnectionActivate:
            return "ConnectionActivate";
        case PoolEvents::ConnectionStop:
            return "ConnectionStop";
        case PoolEvents::PoolDisconnect:
            return "PoolDisconnect";
        case PoolEvents::PoolStop:
            return "PoolStop";
        default:
            return "unknown";
        }
    }};
} // namespace
const LoggerT& service_logger()
{
    return logger;
}
//=============================================================================
// using NameMapT      = std::unordered_map<std::string, ConnectionPoolId>;
using NameMapT      = std::unordered_map<const char*, ConnectionPoolId, CStringHash, CStringEqual>;
using ActorIdQueueT = Queue<ActorIdT>;

/*extern*/ const Event pool_event_connection_start    = make_event(pool_event_category, PoolEvents::ConnectionStart);
/*extern*/ const Event pool_event_connection_activate = make_event(pool_event_category, PoolEvents::ConnectionActivate);
/*extern*/ const Event pool_event_connection_stop     = make_event(pool_event_category, PoolEvents::ConnectionStop);
/*extern*/ const Event pool_event_pool_disconnect     = make_event(pool_event_category, PoolEvents::PoolDisconnect);
/*extern*/ const Event pool_event_pool_stop           = make_event(pool_event_category, PoolEvents::PoolStop);

enum struct MessageInnerLink {
    Order = 0,
    Async,
    Count
};

//-----------------------------------------------------------------------------

struct MessageStub : inner::Node<to_underlying(MessageInnerLink::Count)> {

    enum {
        CancelableFlag = 1
    };

    MessageBundle msgbundle;
    MessageId     msgid;
    ActorIdT      actid;
    uint32_t      unique;
    uint          flags;

    MessageStub(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        ulong                     _msgflags,
        std::string&&             _rmsg_url)
        : msgbundle(_rmsgptr, _msg_type_idx, _msgflags, _rcomplete_fnc, std::move(_rmsg_url))
        , unique(0)
        , flags(0)
    {
    }

    MessageStub(
        MessageStub&& _rmsg) noexcept
        : inner::Node<to_underlying(MessageInnerLink::Count)>(std::move(_rmsg))
        , msgbundle(std::move(_rmsg.msgbundle))
        , msgid(_rmsg.msgid)
        , actid(_rmsg.actid)
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
        actid = ActorIdT();
        ++unique;
        flags = 0;
    }
};

//-----------------------------------------------------------------------------

using MessageVectorT         = std::vector<MessageStub>;
using MessageOrderInnerListT = inner::List<MessageVectorT, to_underlying(MessageInnerLink::Order)>;
using MessageCacheInnerListT = inner::List<MessageVectorT, to_underlying(MessageInnerLink::Order)>;
using MessageAsyncInnerListT = inner::List<MessageVectorT, to_underlying(MessageInnerLink::Async)>;

//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, const MessageOrderInnerListT& _rlst)
{
    size_t cnt = 0;
    _rlst.forEach(
        [&_ros, &cnt](size_t _idx, const MessageStub& _rmsg) {
            _ros << _idx << ' ';
            ++cnt;
        });
    solid_assert_log(cnt == _rlst.size(), logger);
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
    solid_assert_log(cnt == _rlst.size(), logger);
    return _ros;
}

//-----------------------------------------------------------------------------
enum struct ConnectionPoolInnerLink {
    Free = 0,
    Count
};

struct ConnectionPoolStub : inner::Node<to_underlying(ConnectionPoolInnerLink::Count)> {
    enum {
        ClosingFlag                = 1,
        FastClosingFlag            = 2,
        MainConnectionStoppingFlag = 4,
        CleanOneShotMessagesFlag   = 8,
        CleanAllMessagesFlag       = 16,
        RestartFlag                = 32,
        MainConnectionActiveFlag   = 64,
        DisconnectedFlag           = 128,
    };

    uint32_t               unique;
    uint16_t               persistent_connection_count;
    uint16_t               pending_connection_count;
    uint16_t               active_connection_count;
    uint16_t               stopping_connection_count;
    std::string            name; // because c_str() pointer is given to connection - name should allways be std::moved
    ActorIdT               main_connection_id;
    MessageVectorT         msgvec;
    MessageOrderInnerListT msgorder_inner_list;
    MessageCacheInnerListT msgcache_inner_list;
    MessageAsyncInnerListT msgasync_inner_list;
    ActorIdQueueT          conn_waitingq;
    uint8_t                flags;
    uint8_t                retry_connect_count;
    AddressVectorT         connect_addr_vec;
    PoolOnEventFunctionT   on_event_fnc;

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
        ConnectionPoolStub&& _rpool) noexcept
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
        solid_assert_log(msgcache_inner_list.size() == msgvec.size(), logger);
    }

    void clear()
    {
        name.clear();
        main_connection_id = ActorIdT();
        ++unique;
        persistent_connection_count = 0;
        pending_connection_count    = 0;
        active_connection_count     = 0;
        stopping_connection_count   = 0;
        while (!conn_waitingq.empty()) {
            conn_waitingq.pop();
        }
        solid_assert_log(msgorder_inner_list.empty(), logger);
        solid_assert_log(msgasync_inner_list.empty(), logger);
        msgcache_inner_list.clear();
        msgorder_inner_list.clear();
        msgasync_inner_list.clear();
        msgvec.clear();
        msgvec.shrink_to_fit();
        flags               = 0;
        retry_connect_count = 0;
        connect_addr_vec.clear();
        solid_function_clear(on_event_fnc);
        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    MessageId insertMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&&             _msg_url)
    {
        size_t idx;

        if (!msgcache_inner_list.empty()) {
            idx = msgcache_inner_list.popFront();
        } else {
            idx = msgvec.size();
            msgvec.push_back(MessageStub{});
        }

        MessageStub& rmsgstub(msgvec[idx]);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, std::move(_msg_url));

        // solid_assert_log(rmsgstub.msgbundle.message_ptr.get(), logger);

        return MessageId(idx, rmsgstub.unique);
    }

    MessageId pushBackMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&&             _msg_url,
        bool&                     _ris_first)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, std::move(_msg_url));

        _ris_first = msgorder_inner_list.empty();
        msgorder_inner_list.pushBack(msgid.index);

        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushBack(msgid.index);
            solid_log(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        solid_assert_log(msgorder_inner_list.check(), logger);

        return msgid;
    }

    MessageId pushFrontMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&&             _msg_url)
    {
        const MessageId msgid = insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, std::move(_msg_url));

        msgorder_inner_list.pushFront(msgid.index);

        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(msgid.index);
            solid_log(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        solid_assert_log(msgorder_inner_list.check(), logger);

        return msgid;
    }

    MessageId reinsertFrontMessage(
        MessageId const&          _rmsgid,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&&             _msg_url)
    {
        MessageStub& rmsgstub(msgvec[_rmsgid.index]);

        solid_assert_log(!rmsgstub.msgbundle.message_ptr && rmsgstub.unique == _rmsgid.unique, logger);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, std::move(_msg_url));

        msgorder_inner_list.pushFront(_rmsgid.index);

        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(_rmsgid.index);
            solid_log(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        solid_assert_log(msgorder_inner_list.check(), logger);
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
        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgcache_inner_list.pushBack(msgorder_inner_list.popFront());

        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    void popFrontMessage()
    {
        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.popFront();

        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    void eraseMessageOrder(const size_t _msg_idx)
    {
        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    void eraseMessageOrderAsync(const size_t _msg_idx)
    {
        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        const MessageStub& rmsgstub(msgvec[_msg_idx]);
        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
            solid_assert_log(msgasync_inner_list.contains(_msg_idx), logger);
            msgasync_inner_list.erase(_msg_idx);
        }
        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    void clearPopAndCacheMessage(const size_t _msg_idx)
    {
        solid_log(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        MessageStub& rmsgstub(msgvec[_msg_idx]);

        msgorder_inner_list.erase(_msg_idx);

        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
            msgasync_inner_list.erase(_msg_idx);
        }

        msgcache_inner_list.pushBack(_msg_idx);

        rmsgstub.clear();
        solid_assert_log(msgorder_inner_list.check(), logger);
    }

    void cacheMessageId(const MessageId& _rmsgid)
    {
        solid_assert_log(_rmsgid.index < msgvec.size(), logger);
        solid_assert_log(msgvec[_rmsgid.index].unique == _rmsgid.unique, logger);
        if (_rmsgid.index < msgvec.size() && msgvec[_rmsgid.index].unique == _rmsgid.unique) {
            msgvec[_rmsgid.index].clear();
            msgcache_inner_list.pushBack(_rmsgid.index);
        }
    }

    bool isMainConnectionStopping() const
    {
        return (flags & MainConnectionStoppingFlag) != 0u;
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
        return (flags & MainConnectionActiveFlag) != 0u;
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
        return (flags & ClosingFlag) != 0u;
    }

    void setClosing()
    {
        flags |= ClosingFlag;
    }
    bool isFastClosing() const
    {
        return (flags & FastClosingFlag) != 0u;
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
        return (flags & CleanOneShotMessagesFlag) != 0u;
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
        return (flags & CleanAllMessagesFlag) != 0u;
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
        return (flags & RestartFlag) != 0u;
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
        }
        return !msgorder_inner_list.front().msgbundle.message_ptr;
    }

    bool hasAnyMessage() const
    {
        return !hasNoMessage();
    }

    bool isMainConnection(ActorIdT const& _ractuid) const
    {
        return main_connection_id == _ractuid;
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

    bool isDisconnected() const
    {
        return (flags & DisconnectedFlag) != 0u;
    }

    void setDisconnected()
    {
        flags |= DisconnectedFlag;
    }

    void resetDisconnected()
    {
        flags &= (~DisconnectedFlag);
    }
};

//-----------------------------------------------------------------------------

using ConnectionPoolDequeT     = std::deque<ConnectionPoolStub>;
using ConnectionPoolInnerListT = inner::List<ConnectionPoolDequeT, to_underlying(ConnectionPoolInnerLink::Free)>;

//-----------------------------------------------------------------------------

struct Service::Data {
    std::mutex&              mtx;
    const Configuration      config;
    std::mutex*              pmtxarr;
    size_t                   mtxsarrcp;
    NameMapT                 namemap;
    ConnectionPoolDequeT     pooldq;
    ConnectionPoolInnerListT pool_free_list_;
    std::string              tmp_str;

    Data(Service& _rsvc, Configuration&& _config)
        : mtx(_rsvc.mutex())
        , config(std::move(_config)) /*, status(Status::Running)*/
        , pmtxarr(nullptr)
        , mtxsarrcp(0)
        , pool_free_list_(pooldq)
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
#if 0 // TODO:delete
    ErrorConditionT doSendMessageToNewPool(
        Service&                  _rsvc,
        const size_t              _pool_index,
        const std::string_view&   _recipient_url,
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsguid_out,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url);
#endif
    ErrorConditionT doSendMessageToConnection(
        Service&                  _rsvc,
        const RecipientId&        _rrecipient_id_in,
        MessagePointerT&          _rmsgptr,
        MessageCompleteFunctionT& _rcomplete_fnc,
        MessageId*                _pmsg_id_out,
        MessageFlagsT             _flags,
        std::string&&             _msg_url);

    bool doTryCreateNewConnectionForPool(Service& _rsvc, const size_t _pool_index, ErrorConditionT& _rerror);

    bool doNonMainConnectionStopping(
        Service&    _rsvc,
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _pmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingNotLast(
        Service&    _rsvc,
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong&      _rseconds_to_wait,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_pmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingCleanOneShot(
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _rmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingCleanAll(
        Connection& _rcon, ActorIdT const& _ractuid,
        ulong&           _rseconds_to_wait,
        MessageId&       _rmsg_id,
        MessageBundle*   _rmsg_bundle,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionStoppingPrepareCleanOneShot(
        Service&    _rsvc,
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event& _revent_context,
        ErrorConditionT& /*_rerror*/);

    bool doMainConnectionStoppingPrepareCleanAll(
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);

    bool doMainConnectionRestarting(
        Service&    _rsvc,
        Connection& _rcon, ActorIdT const& /*_ractuid*/,
        ulong& /*_rseconds_to_wait*/,
        MessageId& /*_rmsg_id*/,
        MessageBundle* /*_rmsg_bundle*/,
        Event&           _revent_context,
        ErrorConditionT& _rerror);
    void doFetchResendableMessagesFromConnection(
        Service&    _rsvc,
        Connection& _rcon);
    void doPushFrontMessageToPool(
        const ConnectionPoolId& _rpool_id,
        MessageBundle&          _rmsgbundle,
        MessageId const&        _rmsgid);
    ErrorConditionT doLockPool(
        Service& _rsvc, const bool _check_uid, const char* _recipient_name,
        ConnectionPoolId& _rpool_id, unique_lock<std::mutex>& _rlock);
    bool doTryNotifyPoolWaitingConnection(Service& _rsvc, const size_t _pool_index);

    ErrorConditionT doSendMessageToPool(
        Service& _rsvc, const ConnectionPoolId& _rpool_id, MessagePointerT& _rmsgptr,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const size_t              _msg_type_idx,
        std::string&&             _message_url,
        RecipientId*              _precipient_id_out,
        MessageId*                _pmsgid_out,
        const MessageFlagsT&      _flags);
};
//=============================================================================

Service::Service(
    UseServiceShell _force_shell)
    : BaseT(std::move(_force_shell), false)
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
    solid_assert(pimpl_);
    return pimpl_->config;
}

inline std::shared_ptr<Service::Data> Service::acquire(std::unique_lock<std::mutex>& _lock)
{
    if (status(_lock) == ServiceStatusE::Running) {
        return pimpl_;
    }
    return nullptr;
}
//-----------------------------------------------------------------------------
void Service::doStart(ServiceStartStatus& _status, Configuration&& _ucfg)
{
    Configuration cfg;
    SocketDevice  sd;

    cfg.reset(std::move(_ucfg));
    cfg.check();
    cfg.prepare();
    cfg.createListenerDevice(sd);

    Service::doStartWithoutAny(
        [this, &cfg, &sd, &_status](std::unique_lock<std::mutex>& _lock) {
            doFinalizeStart(_status, std::move(cfg), std::move(sd), _lock);
        });
}
//-----------------------------------------------------------------------------
void Service::doStart(ServiceStartStatus& _status)
{
    Service::doStartWithoutAny(
        [this, &_status](std::unique_lock<std::mutex>& _lock) {
            doFinalizeStart(_status, _lock);
        });
}
//-----------------------------------------------------------------------------
void Service::doFinalizeStart(ServiceStartStatus& _status, Configuration&& _ucfg, SocketDevice&& _usd, std::unique_lock<std::mutex>& _lock)
{
    solid_assert(_lock.owns_lock());

    pimpl_ = std::make_shared<Data>(*this, std::move(_ucfg));

    if (configuration().pools_mutex_count > pimpl_->mtxsarrcp) {
        delete[] pimpl_->pmtxarr;
        pimpl_->pmtxarr   = new std::mutex[configuration().pools_mutex_count];
        pimpl_->mtxsarrcp = configuration().pools_mutex_count;
    }

    pimpl_->pooldq.resize(configuration().pools_count);

    pimpl_->pool_free_list_.clear();

    for (size_t i = 0; i < configuration().pools_count; ++i) {
        pimpl_->pool_free_list_.pushBack(i);
    }

    if (_usd) {
        SocketAddress local_address;

        _usd.localAddress(local_address); // socket is moved onto listener

        _lock.unlock(); // temporary unlock the mutex so we can create the listener Actor

        ErrorConditionT error;
        ActorIdT        conuid = _ucfg.scheduler().startActor(make_shared<Listener>(_usd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        _lock.lock();

        solid_check_log(!error, logger, "Failed starting listener: " << error.message());

        _status.listen_addr_vec_.emplace_back(std::move(local_address));
    }

#if 0
    lock_guard<std::mutex> lock(pimpl_->mtx);

    if (_usd) {
        SocketAddress local_address;

        _usd.localAddress(local_address);

        pimpl_ = std::make_shared<Data>(*this, std::move(_ucfg));

        ErrorConditionT error;
        ActorIdT        conuid = _ucfg.scheduler().startActor(make_shared<Listener>(_usd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        if (error) {
            pimpl_->config.clear(std::move(_ucfg));

            solid_throw_log(logger, "Failed to start listener: " << error.message());
        }
        pimpl_->config.prepare();
        pimpl_->config.server.listener_port = local_address.port();
    } else {
        pimpl_->config.reset(std::move(_ucfg));
    }

    if (configuration().pools_mutex_count > pimpl_->mtxsarrcp) {
        delete[] pimpl_->pmtxarr;
        pimpl_->pmtxarr   = new std::mutex[configuration().pools_mutex_count];
        pimpl_->mtxsarrcp = configuration().pools_mutex_count;
    }
#endif
}
//-----------------------------------------------------------------------------
void Service::doFinalizeStart(ServiceStartStatus& _status, std::unique_lock<std::mutex>& _lock)
{
    solid_assert(_lock);
    SocketDevice sd;

    pimpl_->config.createListenerDevice(sd);

    solid_check(pimpl_->pooldq.size() == pimpl_->pool_free_list_.size() && !pimpl_->pooldq.empty());

    if (sd) {
        SocketAddress local_address;

        sd.localAddress(local_address);

        _lock.unlock();

        ErrorConditionT error;
        ActorIdT        conuid = configuration().scheduler().startActor(make_shared<Listener>(sd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        _lock.lock();

        solid_check_log(!error, logger, "Failed starting listener: " << error.message());

        _status.listen_addr_vec_.emplace_back(std::move(local_address));
    }

#if 0
    lock_guard<std::mutex> lock(pimpl_->mtx);
    SocketDevice           sd;

    pimpl_->config.prepare(sd);

    if (sd) {
        SocketAddress local_address;

        sd.localAddress(local_address);

        ErrorConditionT error;
        ActorIdT        conuid = configuration().scheduler().startActor(make_shared<Listener>(sd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        solid_check_log(!error, logger, "Failed starting listener: " << error.message());

        pimpl_->config.server.listener_port = local_address.port();
    }
#endif
}

//-----------------------------------------------------------------------------
#if 0
size_t Service::doPushNewConnectionPool()
{
    solid_log(logger, Verbose, this);

    pimpl_->lockAllConnectionPoolMutexes();

    for (size_t i = 0; i < pimpl_->mtxsarrcp; ++i) {
        const size_t pool_idx = pimpl_->pooldq.size();
        pimpl_->pooldq.push_back(ConnectionPoolStub());
        pimpl_->conpoolcachestk.push(pool_idx);
    }
    pimpl_->unlockAllConnectionPoolMutexes();
    size_t idx = pimpl_->conpoolcachestk.top();
    pimpl_->conpoolcachestk.pop();
    return idx;
}
#endif

//-----------------------------------------------------------------------------

struct OnRelsolveF {

    Manager& rm;
    ActorIdT actuid;
    Event    event;

    OnRelsolveF(
        Manager&        _rm,
        const ActorIdT& _ractuid,
        const Event&    _revent)
        : rm(_rm)
        , actuid(_ractuid)
        , event(_revent)
    {
    }

    void operator()(AddressVectorT&& _raddrvec)
    {
        solid_log(logger, Info, "OnResolveF(addrvec of size " << _raddrvec.size() << ")");
        event.any().emplace<ResolveMessage>(std::move(_raddrvec));
        rm.notify(actuid, std::move(event));
    }
};

//-----------------------------------------------------------------------------

ErrorConditionT Service::doCreateConnectionPool(
    const std::string_view& _recipient_url,
    RecipientId&            _rrecipient_id_out,
    PoolOnEventFunctionT&   _event_fnc,
    const size_t            _persistent_connection_count)
{
    static constexpr const char* empty_recipient_name = ":";
    std::string                  message_url;
    shared_ptr<Data>             locked_pimpl;
    ConnectionPoolId             pool_id;
    const char*                  recipient_name = empty_recipient_name;
    {
        unique_lock<std::mutex> lock;

        locked_pimpl = acquire(lock);

        if (locked_pimpl) {
        } else {
            solid_log(logger, Error, this << " service not running");
            return error_service_stopping;
        }

        recipient_name = configuration().extract_recipient_name_fnc(_recipient_url.data(), message_url, locked_pimpl->tmp_str);

        if (recipient_name == nullptr) {
            solid_log(logger, Error, this << " failed extracting recipient name");
            return error_service_invalid_url;
        } else if (recipient_name[0] == '\0') {
            recipient_name = empty_recipient_name;
        }

        if (pimpl_->namemap.find(recipient_name) == pimpl_->namemap.end()) {
            // pool does not exist
            if (!pimpl_->pool_free_list_.empty()) {
                const auto          pool_index{locked_pimpl->pool_free_list_.popFront()};
                ConnectionPoolStub& rpool(pimpl_->pooldq[pool_index]);

                pool_id                                   = ConnectionPoolId{pool_index, rpool.unique};
                rpool.name                                = recipient_name;
                locked_pimpl->namemap[rpool.name.c_str()] = pool_id;
            } else {
                return error_service_connection_pool_count;
            }
        } else {
            return error_service_pool_exists;
        }
    }

    unique_lock<std::mutex> pool_lock;
    auto                    error = locked_pimpl->doLockPool(*this, false, recipient_name, pool_id, pool_lock);
    if (!error) {
    } else {
        return error;
    }

    solid_assert(pool_lock.owns_lock());

    ConnectionPoolStub& rpool(locked_pimpl->pooldq[pool_id.index]);

    rpool.persistent_connection_count = static_cast<uint16_t>(_persistent_connection_count);
    rpool.on_event_fnc                = std::move(_event_fnc);

    solid_check_log(locked_pimpl->doTryCreateNewConnectionForPool(*this, pool_id.index, error), logger, "doTryCreateNewConnectionForPool failed " << error.message());

    _rrecipient_id_out.pool_id_ = pool_id;
    return error;
#if 0
    solid::ErrorConditionT error;
    size_t                 pool_index;
    std::string            message_url;

    lock_guard<std::mutex> lock(pimpl_->mtx);
    const char*            recipient_name = configuration().extract_recipient_name_fnc(_recipient_url.data(), message_url, pimpl_->tmp_str);

    if (recipient_name == nullptr) {
        solid_log(logger, Error, this << " failed to extract recipient name");
        error = error_service_invalid_url;
        return error;
    } else if (recipient_name[0] == '\0') {
        recipient_name = empty_recipient_name;
    }

    if (pimpl_->namemap.find(recipient_name) == pimpl_->namemap.end()) {
        // pool does not exist
        if (!pimpl_->pool_free_list_.empty()) {
            pool_index = pimpl_->pool_free_list_.popFront();
        } else {
            error = error_service_connection_pool_count;
            return error;
        }

        lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

        rpool.name                        = recipient_name;
        rpool.persistent_connection_count = static_cast<uint16_t>(_persistent_connection_count);
        rpool.on_event_fnc                = std::move(_event_fnc);

        if (!pimpl_->doTryCreateNewConnectionForPool(*this, pool_index, error)) {
            rpool.clear();
            pimpl_->pool_free_list_.pushBack(pool_index);
            return error;
        }

        pimpl_->namemap[rpool.name.c_str()] = pool_index;
        _rrecipient_id_out.pool_id_.index   = pool_index;
        _rrecipient_id_out.pool_id_.unique  = rpool.unique;
    } else {
        error = error_service_pool_exists;
    }

    return error;
#endif
}

//-----------------------------------------------------------------------------
ErrorConditionT Service::Data::doLockPool(
    Service& _rsvc, const bool _check_uid, const char* _recipient_name,
    ConnectionPoolId& _rpool_id, unique_lock<std::mutex>& _rlock)
{
    while (true) {
        _rlock = unique_lock{poolMutex(_rpool_id.index)};
        ConnectionPoolStub& rpool(pooldq[_rpool_id.index]);

        if (rpool.unique == _rpool_id.unique) {
            return ErrorConditionT{};
        } else {
            _rlock.unlock();
            _rlock.release();

            if (_check_uid) {
                solid_log(logger, Error, &_rsvc << " connection pool does not exist");
                return error_service_pool_unknown;
            }

            {
                lock_guard<std::mutex> lock{mtx};

                NameMapT::const_iterator it = namemap.find(_recipient_name);

                if (it != namemap.end()) {
                    _rpool_id = it->second;
                } else {
                    if (!pool_free_list_.empty()) {
                        const auto          pool_index{pool_free_list_.popFront()};
                        ConnectionPoolStub& rpool(pooldq[pool_index]);

                        _rpool_id                   = ConnectionPoolId{pool_index, rpool.unique};
                        rpool.name                  = _recipient_name;
                        namemap[rpool.name.c_str()] = _rpool_id;
                    } else {
                        return error_service_connection_pool_count;
                    }
                }
#if 0
                const auto             itb = namemap.try_emplace(_recipient_name);

                if (!itb.second) {
                    _rpool_id = itb.first->second;
                } else {
                    if (!pool_free_list_.empty()) {
                        const auto pool_index   = pool_free_list_.popFront();
                        _rpool_id               = ConnectionPoolId{pool_index, pooldq[pool_index].unique};
                        pooldq[pool_index].name = itb.first->first;
                    } else {
                        solid_log(logger, Error, &_rsvc << " too many connection pools created");
                        namemap.erase(itb.first);
                        return error_service_connection_pool_count;
                    }

                    itb.first->second = _rpool_id;
                }
#endif
            }
        }
    }
}

ErrorConditionT Service::doSendMessage(
    const char*               _recipient_url,
    const RecipientId&        _rrecipient_id_in,
    MessagePointerT&          _rmsgptr,
    MessageCompleteFunctionT& _rcomplete_fnc,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    const MessageFlagsT&      _flags)
{
    solid_log(logger, Verbose, this);

    std::string      message_url;
    shared_ptr<Data> locked_pimpl;

    if (_rrecipient_id_in.isValidConnection()) {
        if (_rrecipient_id_in.isValidPool()) {
            {
                unique_lock<std::mutex> lock;

                locked_pimpl = acquire(lock);

                if (locked_pimpl) {
                } else {
                    solid_log(logger, Error, this << " service not running");
                    return error_service_stopping;
                }
            }
            return locked_pimpl->doSendMessageToConnection(
                *this,
                _rrecipient_id_in,
                _rmsgptr,
                _rcomplete_fnc,
                _pmsgid_out,
                _flags,
                std::move(message_url));
        } else {
            solid_assert_log(false, logger);
            return error_service_unknown_connection;
        }
    }

    static constexpr const char* empty_recipient_name = ":";
    const char*                  recipient_name       = _recipient_url;
    ConnectionPoolId             pool_id;
    bool                         check_uid = false;
    {
        unique_lock<std::mutex> lock;

        locked_pimpl = acquire(lock);

        if (locked_pimpl) {
        } else {
            solid_log(logger, Error, this << " service not running");
            return error_service_stopping;
        }

        if (_recipient_url != nullptr) {

            recipient_name = configuration().extract_recipient_name_fnc(_recipient_url, message_url, pimpl_->tmp_str);

            if (recipient_name == nullptr) {
                solid_log(logger, Error, this << " failed extracting recipient name");
                return error_service_invalid_url;
            } else if (recipient_name[0] == '\0') {
                recipient_name = empty_recipient_name;
            }

            NameMapT::const_iterator it = locked_pimpl->namemap.find(recipient_name);

            if (it != locked_pimpl->namemap.end()) {
                pool_id = it->second;
            } else {
                if (configuration().isServerOnly()) {
                    solid_log(logger, Error, this << " request for name resolve for a server only configuration");
                    return error_service_server_only;
                }
                if (!pimpl_->pool_free_list_.empty()) {
                    const auto          pool_index{locked_pimpl->pool_free_list_.popFront()};
                    ConnectionPoolStub& rpool(pimpl_->pooldq[pool_index]);

                    pool_id                                   = ConnectionPoolId{pool_index, rpool.unique};
                    rpool.name                                = recipient_name;
                    locked_pimpl->namemap[rpool.name.c_str()] = pool_id;
                } else {
                    return error_service_connection_pool_count;
                }
            }
        } else if (
            static_cast<size_t>(_rrecipient_id_in.pool_id_.index) < pimpl_->pooldq.size()) {
            // we cannot check the uid right now because we need a lock on the pool's mutex
            check_uid = true;
            pool_id   = _rrecipient_id_in.pool_id_;
        } else {
            solid_log(logger, Error, this << " recipient does not exist");
            return error_service_unknown_recipient;
        }
    }

    const size_t msg_type_idx = locked_pimpl->config.protocol().typeIndex(_rmsgptr.get());

    if (msg_type_idx == 0) {
        solid_log(logger, Error, this << " message type not registered");
        return error_service_message_unknown_type;
    }

    unique_lock<std::mutex> pool_lock;
    const auto              error = locked_pimpl->doLockPool(*this, check_uid, recipient_name, pool_id, pool_lock);
    if (!error) {
    } else {
        return error;
    }

    solid_assert(pool_lock.owns_lock());

    return locked_pimpl->doSendMessageToPool(*this, pool_id, _rmsgptr, _rcomplete_fnc, msg_type_idx, std::move(message_url), _precipient_id_out, _pmsgid_out, _flags);
}

//-----------------------------------------------------------------------------

ErrorConditionT Service::Data::doSendMessageToConnection(
    Service&                  _rsvc,
    const RecipientId&        _rrecipient_id_in,
    MessagePointerT&          _rmsgptr,
    MessageCompleteFunctionT& _rcomplete_fnc,
    MessageId*                _pmsgid_out,
    MessageFlagsT             _flags,
    std::string&&             _msg_url)
{
    solid_log(logger, Verbose, &_rsvc);

    solid::ErrorConditionT error;
    const size_t           msg_type_idx = config.protocol().typeIndex(_rmsgptr.get());

    if (msg_type_idx != 0) {
    } else {
        solid_log(logger, Error, &_rsvc << " message type not registered");
        return error_service_message_unknown_type;
    }

#if 0
    if (pool_index >= pimpl_->pooldq.size() || pimpl_->pooldq[pool_index].unique != _rrecipient_id_in.poolId().unique) {
        return error_service_unknown_connection;
    }
#endif

    _flags |= MessageFlagsE::OneShotSend;

    if (Message::is_response_part(_flags) || Message::is_response_last(_flags)) {
        _flags |= MessageFlagsE::Synchronous;
    }

    const size_t pool_index = static_cast<size_t>(_rrecipient_id_in.poolId().index);
    if (pool_index < pooldq.size()) {
    } else {
        solid_log(logger, Error, &_rsvc << " unknown connection");
        return error_service_unknown_connection;
    }
    {
        unique_lock<std::mutex> lock2(poolMutex(pool_index));
        ConnectionPoolStub&     rpool = pooldq[pool_index];

        const bool is_server_side_pool = rpool.isServerSide(); // unnamed pool has a single connection

        if (rpool.unique == _rrecipient_id_in.poolId().unique && !rpool.isClosing() && !rpool.isFastClosing()) {
        } else {
            solid_log(logger, Error, &_rsvc << " unknown connection or connection closing");
            return error_service_unknown_connection;
        }

        if (is_server_side_pool) {
            bool should_notify = false;

            const auto msgid = rpool.pushBackMessage(_rmsgptr, msg_type_idx, _rcomplete_fnc, _flags, std::move(_msg_url), should_notify);

            if (_pmsgid_out != nullptr) {
                *_pmsgid_out          = msgid;
                MessageStub& rmsgstub = rpool.msgvec[msgid.index];
                rmsgstub.makeCancelable();
            }

            lock2.unlock();

            if (should_notify) {
                _rsvc.manager().notify(
                    _rrecipient_id_in.connectionId(),
                    Connection::eventNewMessage());
            }
        } else {

            const auto msgid = rpool.insertMessage(_rmsgptr, msg_type_idx, _rcomplete_fnc, _flags, std::move(_msg_url));

            if (_pmsgid_out != nullptr) {
                *_pmsgid_out          = msgid;
                MessageStub& rmsgstub = rpool.msgvec[msgid.index];
                rmsgstub.makeCancelable();
            }

            lock2.unlock();

            _rsvc.manager().notify(
                _rrecipient_id_in.connectionId(),
                Connection::eventNewMessage(msgid));
        }
    }
#if 0
    if (success) {
        if (_pmsgid_out != nullptr) {
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
#endif
    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::Data::doSendMessageToPool(
    Service& _rsvc, const ConnectionPoolId& _rpool_id, MessagePointerT& _rmsgptr,
    MessageCompleteFunctionT& _rcomplete_fnc,
    const size_t              _msg_type_idx,
    std::string&&             _message_url,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    const MessageFlagsT&      _flags)
{
    solid_log(logger, Verbose, &_rsvc << " " << _rpool_id);

    ConnectionPoolStub& rpool(pooldq[_rpool_id.index]);
    bool                is_first = false;

    if (rpool.isClosing()) {
        solid_log(logger, Error, &_rsvc << " connection pool is stopping");
        return error_service_pool_stopping;
    }

    if (rpool.isFull(config.pool_max_message_queue_size)) {
        solid_log(logger, Error, &_rsvc << " connection pool is full");
        return error_service_pool_full;
    }

    if (_precipient_id_out != nullptr) {
        _precipient_id_out->pool_id_ = _rpool_id;
    }

    // At this point we can fetch the message from user's pointer
    // because from now on we can call complete on the message
    const MessageId msgid = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, std::move(_message_url), is_first);
    (void)is_first;

    if (_pmsgid_out != nullptr) {

        MessageStub& rmsgstub(rpool.msgvec[msgid.index]);

        rmsgstub.makeCancelable();

        *_pmsgid_out = msgid;
        solid_log(logger, Info, &_rsvc << " set message id to " << *_pmsgid_out);
    }

    bool success = false;

    if (
        rpool.isCleaningOneShotMessages() && Message::is_one_shot(_flags)) {
        success = _rsvc.manager().notify(
            rpool.main_connection_id,
            Connection::eventClosePoolMessage(msgid));

        if (success) {
            solid_log(logger, Verbose, &_rsvc << " message " << msgid << " from pool " << _rpool_id << " sent for canceling to " << rpool.main_connection_id);
            // erase/unlink the message from any list
            if (rpool.msgorder_inner_list.contains(msgid.index)) {
                rpool.eraseMessageOrderAsync(msgid.index);
            }
        } else {
            solid_throw_log(logger, "Message Cancel connection not available");
        }
    }

    if (!success && Message::is_synchronous(_flags) && rpool.isMainConnectionActive()) {
        success = _rsvc.manager().notify(
            rpool.main_connection_id,
            Connection::eventNewMessage());
        solid_assert_log(success, logger);
    }

    if (!success && !Message::is_synchronous(_flags)) {
        success = doTryNotifyPoolWaitingConnection(_rsvc, _rpool_id.index);
    }
    ErrorConditionT error;
    if (!success) {
        doTryCreateNewConnectionForPool(_rsvc, _rpool_id.index, error);
        error.clear();
    }

    if (!success) {
        solid_log(logger, Info, &_rsvc << " no connection notified about the new message");
    }
    return error;
}

//-----------------------------------------------------------------------------
#if 0 // TODO:delete
ErrorConditionT Service::Data::doSendMessageToNewPool(
    Service&                  _rsvc,
    const size_t              _pool_index,
    const std::string_view&   _recipient_name,
    MessagePointerT&          _rmsgptr,
    const size_t              _msg_type_idx,
    MessageCompleteFunctionT& _rcomplete_fnc,
    RecipientId*              _precipient_id_out,
    MessageId*                _pmsgid_out,
    const MessageFlagsT&      _flags,
    std::string&              _msg_url)
{

    solid_log(logger, Verbose, this);

    solid::ErrorConditionT error;
    lock_guard<std::mutex> lock2(poolMutex(_pool_index));
    ConnectionPoolStub&    rpool(pooldq[_pool_index]);

    rpool.name = _recipient_name;
    bool      is_first;
    MessageId msgid = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url, is_first);

    if (!doTryCreateNewConnectionForPool(_rsvc, _pool_index, error)) {
        solid_log(logger, Error, this << " Starting Session: " << error.message());
        rpool.popFrontMessage();
        rpool.clear();
        pool_free_list_.pushBack(_pool_index);
        return error;
    }

    if (_precipient_id_out != nullptr) {
        _precipient_id_out->pool_id_ = ConnectionPoolId(_pool_index, rpool.unique);
    }

    if (_pmsgid_out != nullptr) {
        MessageStub& rmsgstub = rpool.msgvec[msgid.index];

        *_pmsgid_out = MessageId(msgid.index, rmsgstub.unique);
        rmsgstub.makeCancelable();
    }

    return error;
}
#endif
//-----------------------------------------------------------------------------
// doTryPushMessageToConnection will accept a message when:
// there is space in the sending queue and
//  either the message is not waiting for response or there is space in the waiting response message queue
bool Service::doTryPushMessageToConnection(
    Connection&     _rcon,
    ActorIdT const& _ractuid,
    const size_t    _pool_index,
    const size_t    _msg_idx)
{
    solid_log(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(pimpl_->pooldq[_pool_index]);
    MessageStub&        rmsgstub                = rpool.msgvec[_msg_idx];
    const bool          message_is_asynchronous = Message::is_asynchronous(rmsgstub.msgbundle.message_flags);
    const bool          message_is_null         = !rmsgstub.msgbundle.message_ptr;
    bool                success                 = false;

    solid_assert_log(!Message::is_canceled(rmsgstub.msgbundle.message_flags), logger);

    if (!rmsgstub.msgbundle.message_ptr) {
        return false;
    }

    if (rmsgstub.isCancelable()) {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, MessageId(_msg_idx, rmsgstub.unique));

        if (success && !message_is_null) {

            rmsgstub.actid = _ractuid;

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
    ActorIdT const&  _ractuid,
    const size_t     _pool_index,
    const MessageId& _rmsg_id)
{
    solid_log(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(pimpl_->pooldq[_pool_index]);

    MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
    bool         success  = false;

    solid_assert_log(!Message::is_canceled(rmsgstub.msgbundle.message_flags), logger);

    if (rmsgstub.isCancelable()) {

        success = _rcon.tryPushMessage(configuration(), rmsgstub.msgbundle, rmsgstub.msgid, _rmsg_id);

        if (success) {
            rmsgstub.actid = _ractuid;
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
    ActorIdT const&  _ractuid,
    MessageId const& _rcompleted_msgid)
{
    solid_log(logger, Verbose, this << " " << &_rconnection);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    ErrorConditionT error;

    solid_assert_log(rpool.unique == _rconnection.poolId().unique, logger);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_pool_unknown;
        return error;
    }

    if (rpool.isMainConnectionStopping() && rpool.main_connection_id != _ractuid) {

        solid_log(logger, Info, this << ' ' << &_rconnection << " switch message main connection from " << rpool.main_connection_id << " to " << _ractuid);

        manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());

        rpool.main_connection_id = _ractuid;
        rpool.resetMainConnectionStopping();
        rpool.setMainConnectionActive();
    }

    if (_rcompleted_msgid.isValid()) {
        rpool.cacheMessageId(_rcompleted_msgid);
    }

    if (rpool.isFastClosing()) {
        solid_log(logger, Info, this << ' ' << &_rconnection << " pool is FastClosing");
        error = error_service_pool_stopping;
        return error;
    }

    if (_rconnection.isWriterEmpty() && rpool.shouldClose()) {
        solid_log(logger, Info, this << ' ' << &_rconnection << " pool is DelayClosing");
        error = error_service_pool_stopping;
        return error;
    }

    solid_log(logger, Info, this << ' ' << &_rconnection << " messages in pool: " << rpool.msgorder_inner_list);

    bool       connection_may_handle_more_messages        = !_rconnection.isFull(configuration());
    const bool connection_can_handle_synchronous_messages = _ractuid == rpool.main_connection_id;

    // We need to push as many messages as we can to the connection
    // in order to handle eficiently the situation with multiple small messages.

    if (!_rconnection.pendingMessageVector().empty()) {
        // connection has pending messages
        // fetch as many as we can
        size_t count = 0;

        for (const auto& rmsgid : _rconnection.pendingMessageVector()) {
            if (rmsgid.index < rpool.msgvec.size() && rmsgid.unique == rpool.msgvec[rmsgid.index].unique) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(_rconnection, _ractuid, pool_index, rmsgid);
                if (connection_may_handle_more_messages) {
                } else {
                    break;
                }
            } else {
                solid_assert_log(false, logger);
            }
            ++count;
        }
        if (count != 0u) {
            _rconnection.pendingMessageVectorEraseFirst(count);
        }
    }
    if (_rconnection.isServer() || _rconnection.isActiveState()) {
        if (connection_can_handle_synchronous_messages) {
            // use the order inner queue
            while (!rpool.msgorder_inner_list.empty() && connection_may_handle_more_messages) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(
                    _rconnection,
                    _ractuid,
                    pool_index,
                    rpool.msgorder_inner_list.frontIndex());
            }

        } else {

            // use the async inner queue
            while (!rpool.msgasync_inner_list.empty() && connection_may_handle_more_messages) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(
                    _rconnection,
                    _ractuid,
                    pool_index,
                    rpool.msgasync_inner_list.frontIndex());
            }
        }
        // a connection will either be in conn_waitingq
        // or it will call pollPoolForUpdates asap.
        // this is because we need to be able to notify connection about
        // pool force close imeditely
        if (!_rconnection.isInPoolWaitingQueue()) {
            rpool.conn_waitingq.push(_ractuid);
            _rconnection.setInPoolWaitingQueue();
        }
    } // if active state
    return error;
}
//-----------------------------------------------------------------------------
void Service::rejectNewPoolMessage(Connection const& _rconnection)
{
    solid_log(logger, Verbose, this);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    solid_assert_log(rpool.unique == _rconnection.poolId().unique, logger);
    if (rpool.unique != _rconnection.poolId().unique) {
        return;
    }

    pimpl_->doTryNotifyPoolWaitingConnection(*this, pool_index);
}
//-----------------------------------------------------------------------------
bool Service::Data::doTryNotifyPoolWaitingConnection(Service& _rsvc, const size_t _pool_index)
{
    solid_log(logger, Verbose, &_rsvc << " " << _pool_index);

    ConnectionPoolStub& rpool(pooldq[_pool_index]);
    bool                success = false;

    // we were not able to handle the message, try notify another connection
    while (!success && !rpool.conn_waitingq.empty()) {
        // a connection is waiting for something to send
        ActorIdT actuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        success = _rsvc.manager().notify(
            actuid,
            Connection::eventNewQueueMessage());
    }
    return success;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doDelayCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();
    solid_log(logger, Verbose, this << " pool " << pool_index << " set closing");

    {
        lock_guard<std::mutex> lock(pimpl_->mtx);
        if (!rpool.name.empty()) {
            pimpl_->namemap.erase(rpool.name.c_str());
        }
    }

    MessagePointerT empty_msg_ptr;
    bool            is_first;
    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, 0, std::string{}, is_first);
    (void)msgid;

    // notify all waiting connections about the new message
    while (!rpool.conn_waitingq.empty()) {
        ActorIdT actuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            actuid,
            Connection::eventNewQueueMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doForceCloseConnectionPool(
    RecipientId const&        _rrecipient_id,
    MessageCompleteFunctionT& _rcomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    if (pool_index >= pimpl_->pooldq.size() || rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    rpool.setClosing();
    rpool.setFastClosing();

    solid_log(logger, Verbose, this << " pool " << pool_index << " set fast closing");
    {
        lock_guard<std::mutex> lock(pimpl_->mtx);
        
        if (!rpool.name.empty()) {
            pimpl_->namemap.erase(rpool.name.c_str());
        }
    }

    MessagePointerT empty_msg_ptr;
    bool            is_first;
    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, {MessageFlagsE::Synchronous}, std::string{}, is_first);
    (void)msgid;

    // no reason to cancel all messages - they'll be handled on connection stop.
    // notify all waiting connections about the new message
    while (!rpool.conn_waitingq.empty()) {
        ActorIdT actuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        manager().notify(
            actuid,
            Connection::eventNewQueueMessage());
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::cancelMessage(RecipientId const& _rrecipient_id, MessageId const& _rmsg_id)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT        error;
    const size_t           pool_index = static_cast<size_t>(_rrecipient_id.poolId().index);
    lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    if (rpool.unique != _rrecipient_id.poolId().unique) {
        return error_service_unknown_connection;
    }

    if (_rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
        bool         success  = false;

        if (Message::is_canceled(rmsgstub.msgbundle.message_flags)) {
            error = error_service_message_already_canceled;
        } else {

            if (rmsgstub.actid.isValid()) { // message handled by a connection

                solid_log(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " is handled by connection " << rmsgstub.actid);

                solid_assert_log(!rmsgstub.msgbundle.message_ptr, logger);

                rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);

                success = manager().notify(
                    rmsgstub.actid,
                    Connection::eventCancelConnMessage(rmsgstub.msgid));

                if (success) {
                    rmsgstub.clear();
                    rpool.msgcache_inner_list.pushBack(_rmsg_id.index);
                } else {
                    rmsgstub.msgid = MessageId();
                    rmsgstub.actid = ActorIdT();
                    error          = error_service_message_lost;
                    solid_throw_log(logger, "Lost message");
                }
            }

            if (rmsgstub.msgbundle.message_ptr) {

                solid_log(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " not handled by any connection");

                rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);

                success = manager().notify(
                    rpool.main_connection_id,
                    Connection::eventCancelPoolMessage(_rmsg_id));

                if (success) {
                    solid_log(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " sent for canceling to " << rpool.main_connection_id);
                    // erase/unlink the message from any list
                    if (rpool.msgorder_inner_list.contains(_rmsg_id.index)) {
                        rpool.eraseMessageOrderAsync(_rmsg_id.index);
                    }
                } else {
                    solid_throw_log(logger, "Message Cancel connection not available");
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
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterActive(std::move(_ucomplete_fnc), _send_buffer_capacity));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify enter active event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
void Service::doConnectionNotifyPostAll(
    ConnectionPostCompleteFunctionT&& _ucomplete_fnc)
{
    solid_log(logger, Verbose, this);

    this->notifyAll(Connection::eventPost(std::move(_ucomplete_fnc)));
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyPost(
    RecipientId const&                _rrecipient_id,
    ConnectionPostCompleteFunctionT&& _ucomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventPost(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify enter active event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyStartSecureHandshake(
    RecipientId const&                          _rrecipient_id,
    ConnectionSecureHandhakeCompleteFunctionT&& _ucomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventStartSecure(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify start secure event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyEnterPassiveState(
    RecipientId const&                        _rrecipient_id,
    ConnectionEnterPassiveCompleteFunctionT&& _ucomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventEnterPassive(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify enter passive event to " << _rrecipient_id.connectionId());
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
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventSendRaw(std::move(_ucomplete_fnc), std::move(_rdata)));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify send raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::doConnectionNotifyRecvRawData(
    RecipientId const&                       _rrecipient_id,
    ConnectionRecvRawDataCompleteFunctionT&& _ucomplete_fnc)
{
    solid_log(logger, Verbose, this);

    ErrorConditionT error;
    bool            success = manager().notify(
        _rrecipient_id.connectionId(),
        Connection::eventRecvRaw(std::move(_ucomplete_fnc)));

    if (!success) {
        solid_log(logger, Warning, this << " failed notify recv raw event to " << _rrecipient_id.connectionId());
        error = error_service_unknown_connection;
    }

    return error;
}
//-----------------------------------------------------------------------------
bool Service::fetchMessage(Connection& _rcon, ActorIdT const& _ractuid, MessageId const& _rmsg_id)
{
    solid_log(logger, Verbose, this);

    lock_guard<std::mutex> lock2(pimpl_->poolMutex(static_cast<size_t>(_rcon.poolId().index)));
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        return doTryPushMessageToConnection(_rcon, _ractuid, pool_index, _rmsg_id.index);
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::fetchCanceledMessage(Connection const& _rcon, MessageId const& _rmsg_id, MessageBundle& _rmsg_bundle)
{
    solid_log(logger, Verbose, this);

    lock_guard<std::mutex> lock2(pimpl_->poolMutex(static_cast<size_t>(_rcon.poolId().index)));
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];

        if (rpool.msgorder_inner_list.contains(_rmsg_id.index)) {
            rpool.eraseMessageOrderAsync(_rmsg_id.index);
        }

        // rmsgstub.msgbundle.message_flags.set(MessageFlagsE::Canceled);
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
    solid_log(logger, Verbose, this);

    return manager().notify(
        _rrecipient_id.connectionId(),
        make_event(GenericEvents::Kill));
}
//-----------------------------------------------------------------------------
bool Service::connectionStopping(
    ConnectionContext& _rconctx,
    ActorIdT const&    _ractuid,
    ulong&             _rseconds_to_wait,
    MessageId&         _rmsg_id,
    MessageBundle*     _pmsg_bundle,
    Event&             _revent_context,
    ErrorConditionT&   _rerror)
{
    solid_log(logger, Verbose, this);
    bool                retval;
    ConnectionPoolStub* ppool = nullptr;
    {
        Connection& rcon(_rconctx.connection());
        // TODO:delete
        // lock_guard<std::mutex> lock(pimpl_->mtx);
        const size_t           pool_index = static_cast<size_t>(rcon.poolId().index);
        lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

        _rseconds_to_wait = 0;

        if (_pmsg_bundle != nullptr) {
            _pmsg_bundle->clear();
        }

        solid_assert_log(rpool.unique == rcon.poolId().unique, logger);
        if (rpool.unique != rcon.poolId().unique) {
            return false;
        }

        solid_log(logger, Info, this << ' ' << pool_index << " active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);

        bool was_disconnected = rpool.isDisconnected();

        if (!rpool.isMainConnection(_ractuid)) {
            retval = pimpl_->doNonMainConnectionStopping(*this, rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (!rpool.isLastConnection()) {
            retval = pimpl_->doMainConnectionStoppingNotLast(*this, rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isCleaningOneShotMessages()) {
            retval = pimpl_->doMainConnectionStoppingCleanOneShot(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isCleaningAllMessages()) {
            retval = pimpl_->doMainConnectionStoppingCleanAll(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isRestarting() /*TODO:vapa && running()*/) {
            retval = pimpl_->doMainConnectionRestarting(*this, rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (!rpool.isFastClosing() && !rpool.isServerSide() /*TODO:vapa && running()*/) {
            retval = pimpl_->doMainConnectionStoppingPrepareCleanOneShot(*this, rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else {
            retval = pimpl_->doMainConnectionStoppingPrepareCleanAll(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        }
        if (!was_disconnected && rpool.isDisconnected() && !solid_function_empty(rpool.on_event_fnc)) {
            ppool = &rpool;
        }
    }
    if (ppool != nullptr) {
        // the call is safe because the current method is called from within a connection and
        //  the connection pool entry is released when the last connection calls
        //  Service::connectionStop
        ppool->on_event_fnc(_rconctx, make_event(pool_event_category, PoolEvents::PoolDisconnect), _rerror);
    }
    if (retval && _rconctx.relayId().isValid()) {
        // must call relayEngine.stopConnection here instead-of on Service::connectionStop
        // because at that point, the connection actor cannot receive notifications
        // as the call to Service::connectionStop is made after connection postStop
        // and relay engines requires that all registered connections can be notified.
        // See note on Connection::doHandleEventRelayNew and Connection::doHandleEventRelayDone
        configuration().relayEngine().stopConnection(_rconctx.relayId());
    }
    return retval;
}
//-----------------------------------------------------------------------------
bool Service::Data::Data::doNonMainConnectionStopping(
    Service&    _rsvc,
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* _rmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count << " is active = " << _rcon.isActiveState());

    if (_rmsg_bundle == nullptr) {
        // cannot stop the connection right now - the connection has to clean its messages first
        return false;
    }

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        solid_assert_log(!_rcon.isServer(), logger);
        solid_assert_log(rpool.pending_connection_count >= 0, logger);
        --rpool.pending_connection_count;
    }

    ++rpool.stopping_connection_count;

    if (rpool.isLastConnection() && rpool.isMainConnectionStopping()) {
        _rsvc.manager().notify(
            rpool.main_connection_id,
            Connection::eventStopping());
    }

    if (!rpool.isFastClosing()) {
        doFetchResendableMessagesFromConnection(_rsvc, _rcon);
        ErrorConditionT error;
        doTryCreateNewConnectionForPool(_rsvc, pool_index, error);
    }

    return true; // the connection can call connectionStop asap
}
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionStoppingNotLast(
    Service&    _rsvc,
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong&      _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_pmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    // it's the main connection but it is not the last one

    _rseconds_to_wait = 1;

    rpool.setMainConnectionStopping();
    rpool.resetMainConnectionActive();

    if (!rpool.isFastClosing()) {
        doFetchResendableMessagesFromConnection(_rsvc, _rcon);
    }

    return false;
}
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionStoppingCleanOneShot(
    Connection&    _rcon, ActorIdT const& /*_ractuid*/,
    ulong&         _rseconds_to_wait,
    MessageId&     _rmsg_id,
    MessageBundle* _pmsg_bundle,
    Event&         _revent_context,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    size_t*      pmsgidx   = _revent_context.any().cast<size_t>();
    const size_t crtmsgidx = *pmsgidx;

    if (crtmsgidx != InvalidIndex()) {

        MessageStub& rmsgstub = rpool.msgvec[crtmsgidx];

        *pmsgidx = rpool.msgorder_inner_list.nextIndex(crtmsgidx);

        if (rpool.msgorder_inner_list.size() != 1) {
            solid_assert_log(rpool.msgorder_inner_list.contains(crtmsgidx), logger);
        } else if (rpool.msgorder_inner_list.size() == 1) {
            solid_assert_log(rpool.msgorder_inner_list.frontIndex() == crtmsgidx, logger);
        } else {
            solid_assert_log(false, logger);
        }

        if (rmsgstub.msgbundle.message_ptr && Message::is_one_shot(rmsgstub.msgbundle.message_flags)) {
            *_pmsg_bundle = std::move(rmsgstub.msgbundle);
            _rmsg_id      = MessageId(crtmsgidx, rmsgstub.unique);
            rpool.clearPopAndCacheMessage(crtmsgidx);
        } else if (!rmsgstub.msgbundle.message_ptr && rpool.msgorder_inner_list.size() == 1) {
            *_pmsg_bundle = std::move(rmsgstub.msgbundle);
            _rmsg_id      = MessageId(crtmsgidx, rmsgstub.unique);
            rpool.clearPopAndCacheMessage(crtmsgidx);
        }
        return false;
    }

    rpool.resetCleaningOneShotMessages();
    rpool.setRestarting();
    if (rpool.connect_addr_vec.empty()) {
        _rseconds_to_wait = config.connectionReconnectTimeoutSeconds(
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
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionStoppingCleanAll(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId&     _rmsg_id,
    MessageBundle* _pmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    if (!rpool.msgorder_inner_list.empty()) {
        const size_t msgidx = rpool.msgorder_inner_list.frontIndex();
        {
            MessageStub& rmsgstub = rpool.msgorder_inner_list.front();
            *_pmsg_bundle         = std::move(rmsgstub.msgbundle);
            _rmsg_id              = MessageId(msgidx, rmsgstub.unique);
        }
        rpool.clearPopAndCacheMessage(msgidx);
    }

    if (rpool.msgorder_inner_list.empty()) {
        if (_rcon.isActiveState()) {
            --rpool.active_connection_count;
        } else {
            solid_assert_log(rpool.pending_connection_count >= 0, logger);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;
        rpool.resetCleaningAllMessages();

        if (!rpool.name.empty() && !rpool.isClosing()) { // closing pools are already unregistered from namemap
            namemap.erase(rpool.name.c_str());
            rpool.setClosing();
            solid_log(logger, Verbose, this << " pool " << pool_index << " set closing");
        }

        return true; // TODO: maybe we should return false
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionStoppingPrepareCleanOneShot(
    Service&    _rsvc,
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* _pmsg_bundle,
    Event&         _revent_context,
    ErrorConditionT& /*_rerror*/
)
{
    // the last connection
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    doFetchResendableMessagesFromConnection(_rsvc, _rcon);

    rpool.resetMainConnectionActive();
    rpool.setDisconnected();

    if (!rpool.msgorder_inner_list.empty() || rpool.persistent_connection_count != 0) {
        if (_pmsg_bundle != nullptr) {
            rpool.setCleaningOneShotMessages();

            _revent_context.any() = rpool.msgorder_inner_list.frontIndex();
        }
        return false;
    }

    if (_pmsg_bundle != nullptr) {
        if (_rcon.isActiveState()) {
            --rpool.active_connection_count;
        } else {
            solid_assert_log(rpool.pending_connection_count >= 0, logger);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;

        if (!rpool.name.empty() && !rpool.isClosing()) { // closing pools are already unregistered from namemap
            {
                lock_guard<std::mutex> lock{mtx};
                namemap.erase(rpool.name.c_str());
                rpool.name.clear();
            }
            rpool.setClosing();
            solid_log(logger, Verbose, this << " pool " << pool_index << " set closing");
        }
        return true; // the connection can call connectionStop asap
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionStoppingPrepareCleanAll(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    // the last connection - fast closing or server side
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    if (!rpool.name.empty() && !rpool.isClosing()) { // closing pools are already unregistered from namemap
        lock_guard<std::mutex> lock{mtx};
        namemap.erase(rpool.name.c_str());
        rpool.name.clear();
    }

    rpool.setCleaningAllMessages();
    rpool.resetMainConnectionActive();
    rpool.setFastClosing();
    rpool.setClosing();
    solid_log(logger, Verbose, this << " pool " << pool_index << " set closing");

    return false;
}
//-----------------------------------------------------------------------------
bool Service::Data::doMainConnectionRestarting(
    Service&    _rsvc,
    Connection& _rcon, ActorIdT const& _ractuid,
    ulong& _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(pooldq[pool_index]);

    solid_log(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        solid_assert_log(rpool.pending_connection_count >= 0, logger);
        --rpool.pending_connection_count;
    }

    ++rpool.stopping_connection_count;
    rpool.main_connection_id = ActorIdT();

    if (_rcon.isConnected()) {
        rpool.retry_connect_count = 0;
    }

    if (rpool.hasAnyMessage() || rpool.persistent_connection_count != 0) {

        ErrorConditionT error;
        bool            success = false;
        bool            tried   = false;

        while (!rpool.conn_waitingq.empty()) {
            rpool.conn_waitingq.pop();
        }

        if ((rpool.retry_connect_count & 1) == 0) {
            success = doTryCreateNewConnectionForPool(_rsvc, pool_index, error);
            tried   = true;
        }

        ++rpool.retry_connect_count;

        if (!success) {
            if (_rcon.isActiveState()) {
                ++rpool.active_connection_count;
            } else {
                solid_assert_log(!_rcon.isServer(), logger);
                ++rpool.pending_connection_count;
            }

            --rpool.stopping_connection_count;
            rpool.main_connection_id = _ractuid;

            _rseconds_to_wait = config.connectionReconnectTimeoutSeconds(
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
void Service::connectionStop(ConnectionContext& _rconctx)
{
    solid_log(logger, Info, this << ' ' << &_rconctx.connection());
    PoolOnEventFunctionT on_event_fnc;
    {
        const size_t            pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);
        unique_lock<std::mutex> lock2(pimpl_->poolMutex(pool_index));
        ConnectionPoolStub&     rpool(pimpl_->pooldq[pool_index]);

        solid_assert_log(rpool.unique == _rconctx.connection().poolId().unique, logger);
        if (rpool.unique != _rconctx.connection().poolId().unique) {
            return;
        }

        --rpool.stopping_connection_count;
        rpool.resetRestarting();

        lock2.unlock();

        // do not call callback under lock
        if (!solid_function_empty(rpool.on_event_fnc) && _rconctx.connection().isConnected()) {
            rpool.on_event_fnc(_rconctx, make_event(pool_event_category, PoolEvents::ConnectionStop), ErrorConditionT());
        }

        configuration().connection_stop_fnc(_rconctx);

        lock2.lock();

        // we might need to release the connectionpool entry - so we need the main lock
        lock_guard<std::mutex> lock(pimpl_->mtx);

        if (rpool.hasNoConnection()) {

            solid_assert_log(rpool.msgorder_inner_list.empty(), logger);
            pimpl_->pool_free_list_.pushBack(pool_index);

            if (!rpool.name.empty() && !rpool.isClosing()) { // closing pools are already unregistered from namemap
                pimpl_->namemap.erase(rpool.name.c_str());
            }

            on_event_fnc = std::move(rpool.on_event_fnc);

            rpool.clear();
        }
    }
    if (!solid_function_empty(on_event_fnc)) {
        on_event_fnc(_rconctx, make_event(pool_event_category, PoolEvents::PoolStop), ErrorConditionT());
    }
}
//-----------------------------------------------------------------------------
bool Service::Data::doTryCreateNewConnectionForPool(Service& _rsvc, const size_t _pool_index, ErrorConditionT& _rerror)
{
    solid_log(logger, Verbose, this);

    ConnectionPoolStub& rpool(pooldq[_pool_index]);
    const bool          is_new_connection_needed = rpool.active_connection_count < rpool.persistent_connection_count || (rpool.hasAnyMessage() && rpool.conn_waitingq.size() < rpool.msgorder_inner_list.size());

    if (
        rpool.active_connection_count < config.pool_max_active_connection_count && rpool.pending_connection_count == 0 && is_new_connection_needed /*TODO:vapa && running()*/) {

        solid_log(logger, Info, this << " try create new connection in pool " << rpool.active_connection_count << " pending connections " << rpool.pending_connection_count);

        auto     actptr(new_connection(config, ConnectionPoolId(_pool_index, rpool.unique), rpool.name));
        ActorIdT conuid = config.scheduler().startActor(std::move(actptr), _rsvc, make_event(GenericEvents::Start), _rerror);

        if (!_rerror) {

            solid_log(logger, Info, this << " Success starting Connection Pool actor: " << conuid.index << ',' << conuid.unique);

            ++rpool.pending_connection_count;

            if (rpool.main_connection_id.isInvalid()) {
                rpool.main_connection_id = conuid;
            }

            if (rpool.connect_addr_vec.empty()) {

                ResolveCompleteFunctionT cbk(OnRelsolveF(_rsvc.manager(), conuid, Connection::eventResolve()));

                config.client.name_resolve_fnc(rpool.name, cbk);

            } else {
                // use the rest of the already resolved addresses
                Event event = Connection::eventResolve();

                event.any().emplace<ResolveMessage>(std::move(rpool.connect_addr_vec));

                _rsvc.manager().notify(conuid, std::move(event));
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
    solid_log(logger, Verbose, this);

    ResolveMessage* presolvemsg = _revent.any().cast<ResolveMessage>();
    ErrorConditionT error;

    if (!presolvemsg->addrvec.empty()) {
        lock_guard<std::mutex> lock(pimpl_->poolMutex(static_cast<size_t>(_rpoolid.index)));
        ConnectionPoolStub&    rpool(pimpl_->pooldq[static_cast<size_t>(_rpoolid.index)]);

        if (rpool.pending_connection_count < configuration().pool_max_pending_connection_count) {

            auto     actptr(new_connection(configuration(), _rpoolid, rpool.name));
            ActorIdT conuid = pimpl_->config.scheduler().startActor(std::move(actptr), *this, std::move(_revent), error);

            if (!error) {
                ++rpool.pending_connection_count;

                solid_log(logger, Info, this << ' ' << _rpoolid << " new connection " << conuid << " - active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);
            } else {
                solid_log(logger, Info, this << ' ' << conuid << " " << error.message());
            }
        } else {
            // enough pending connection - cache the addrvec for later use
            rpool.connect_addr_vec = std::move(presolvemsg->addrvec);
        }
    } else {
        lock_guard<std::mutex> lock(pimpl_->poolMutex(static_cast<size_t>(_rpoolid.index)));

        pimpl_->doTryCreateNewConnectionForPool(*this, static_cast<size_t>(_rpoolid.index), error);
    }
}
//-----------------------------------------------------------------------------
void Service::Data::doFetchResendableMessagesFromConnection(
    Service&    _rsvc,
    Connection& _rcon)
{
    solid_log(logger, Verbose, this << " " << &_rcon);
    // the final front message in msgorder_inner_list should be the oldest one from connection
    _rcon.fetchResendableMessages(_rsvc,
        [this](
            const ConnectionPoolId& _rpool_id,
            MessageBundle&          _rmsgbundle,
            MessageId const&        _rmsgid) {
            doPushFrontMessageToPool(_rpool_id, _rmsgbundle, _rmsgid);
        });
}
//-----------------------------------------------------------------------------
void Service::Data::doPushFrontMessageToPool(
    const ConnectionPoolId& _rpool_id,
    MessageBundle&          _rmsgbundle,
    MessageId const&        _rmsgid)
{
    ConnectionPoolStub& rpool(pooldq[static_cast<size_t>(_rpool_id.index)]);

    solid_log(logger, Verbose, this << " " << _rmsgbundle.message_ptr.get() << " msgorder list sz = " << rpool.msgorder_inner_list.size());

    solid_assert_log(rpool.unique == _rpool_id.unique, logger);

    if (
        Message::is_idempotent(_rmsgbundle.message_flags) || !Message::is_done_send(_rmsgbundle.message_flags)) {

        solid_log(logger, Verbose, this << " " << _rmsgbundle.message_ptr.get());

        _rmsgbundle.message_flags.reset(MessageFlagsE::DoneSend).reset(MessageFlagsE::StartedSend);

        if (_rmsgid.isInvalid()) {
            rpool.pushFrontMessage(
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags,
                std::move(_rmsgbundle.message_url));
        } else {
            rpool.reinsertFrontMessage(
                _rmsgid,
                _rmsgbundle.message_ptr,
                _rmsgbundle.message_type_id,
                _rmsgbundle.complete_fnc,
                _rmsgbundle.message_flags,
                std::move(_rmsgbundle.message_url));
        }
    }
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::activateConnection(ConnectionContext& _rconctx, ActorIdT const& _ractuid)
{
    solid_log(logger, Verbose, this);

    const size_t pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);

    ConnectionPoolStub* pconpool = nullptr;
    ErrorConditionT     error;
    {
        lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

        solid_assert_log(rpool.unique == _rconctx.connection().poolId().unique, logger);
        if (rpool.unique != _rconctx.connection().poolId().unique) {
            error = error_service_pool_unknown;
            return error;
        }

        if (_rconctx.connection().isActiveState()) {
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

            rpool.main_connection_id = _ractuid;
            rpool.resetMainConnectionStopping();
            rpool.setMainConnectionActive();

            // return error;
        } else {

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
                pimpl_->doTryCreateNewConnectionForPool(*this, pool_index, err);
            }
        }

        rpool.resetDisconnected();

        if (!solid_function_empty(rpool.on_event_fnc)) {
            pconpool = &rpool;
        }
    }

    if (pconpool != nullptr) {
        pconpool->on_event_fnc(_rconctx, make_event(pool_event_category, PoolEvents::ConnectionActivate), error);
    }
    return error;
}
//-----------------------------------------------------------------------------
void Service::acceptIncomingConnection(SocketDevice& _rsd)
{
    solid_log(logger, Verbose, this);

    configuration().server.socket_device_setup_fnc(_rsd);

    size_t pool_index;
    {
        lock_guard<std::mutex> lock(pimpl_->mtx);

        if (!pimpl_->pool_free_list_.empty()) {
            pool_index = pimpl_->pool_free_list_.popFront();
        } else {
            solid_log(logger, Error, "Too many connection pools created");
            return;
        }
    }

    {
        lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));

        ConnectionPoolStub& rpool(pimpl_->pooldq[pool_index]);

        auto actptr(new_connection(configuration(), _rsd, ConnectionPoolId(pool_index, rpool.unique), rpool.name));

        solid::ErrorConditionT error;

        ActorIdT con_id = pimpl_->config.scheduler().startActor(
            std::move(actptr), *this, make_event(GenericEvents::Start), error);

        solid_log(logger, Info, this << " receive connection [" << con_id << "] error = " << error.message());

        if (error) {
            solid_assert_log(con_id.isInvalid(), logger);
            rpool.clear();
            pimpl_->pool_free_list_.pushBack(pool_index);
        } else {
            solid_assert_log(con_id.isValid(), logger);
            ++rpool.pending_connection_count;
            rpool.main_connection_id = con_id;
        }
    }
}
//-----------------------------------------------------------------------------
void Service::onIncomingConnectionStart(ConnectionContext& _rconctx)
{
    solid_log(logger, Verbose, this);
    configuration().server.connection_start_fnc(_rconctx);
}
//-----------------------------------------------------------------------------
void Service::onOutgoingConnectionStart(ConnectionContext& _rconctx)
{
    solid_log(logger, Verbose, this);
    ConnectionPoolStub* ppool = nullptr;
    {
        const size_t           pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);
        //lock_guard<std::mutex> lock(pimpl_->mtx);
        lock_guard<std::mutex> lock2(pimpl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(pimpl_->pooldq[pool_index]);

        solid_assert_log(rpool.unique == _rconctx.connection().poolId().unique, logger);
        if (rpool.unique != _rconctx.connection().poolId().unique) {
            return;
        }

        if (!solid_function_empty(rpool.on_event_fnc)) {
            ppool = &rpool;
        }
    }

    // first call the more general function
    configuration().client.connection_start_fnc(_rconctx);

    if (ppool != nullptr) {
        // it should be safe using the pool right now because it is used
        //  from within a pool's connection
        ppool->on_event_fnc(_rconctx, make_event(pool_event_category, PoolEvents::ConnectionStart), ErrorConditionT());
    }
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::sendRelay(const ActorIdT& /*_rconid*/, RelayData&& /*_urelmsg*/)
{
    ErrorConditionT error;

    return error;
}
//-----------------------------------------------------------------------------
ErrorConditionT Service::sendRelayCancel(RelayData&& /*_urelmsg*/)
{
    ErrorConditionT error;
    return error;
}
//-----------------------------------------------------------------------------
void Service::onLockedStoppingBeforeActors()
{
    // TODO:vapa
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
                solid_log(logger, Info, "add resolved endpoint: " << addrvec.back() << ':' << addrvec.back().port());
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

    const size_t off = _name.rfind(':');
    if (off != std::string::npos) {
        tmp      = _name.substr(0, off);
        hst_name = tmp.c_str();
        svc_name = _name.c_str() + off + 1;
        if (svc_name[0] == 0) {
            svc_name = default_service_.c_str();
        }
    } else {
        hst_name = _name.c_str();
        svc_name = default_service_.c_str();
    }

    if (hst_name[0] == 0) {
        hst_name = default_host_.c_str();
    }

    ResolveF fnc;

    fnc.cbk = std::move(_cbk);

    rresolver_.requestResolve(fnc, hst_name, svc_name, 0, this->family_, SocketInfo::Stream);
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
} // namespace mprpc
} // namespace frame
} // namespace solid
