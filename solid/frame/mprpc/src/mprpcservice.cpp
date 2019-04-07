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
} //namespace
//=============================================================================
using NameMapT      = std::unordered_map<const char*, size_t, CStringHash, CStringEqual>;
using ActorIdQueueT = Queue<ActorIdT>;

/*extern*/ const Event pool_event_connection_start    = pool_event_category.event(PoolEvents::ConnectionStart);
/*extern*/ const Event pool_event_connection_activate = pool_event_category.event(PoolEvents::ConnectionActivate);
/*extern*/ const Event pool_event_connection_stop     = pool_event_category.event(PoolEvents::ConnectionStop);
/*extern*/ const Event pool_event_pool_disconnect     = pool_event_category.event(PoolEvents::PoolDisconnect);
/*extern*/ const Event pool_event_pool_stop           = pool_event_category.event(PoolEvents::PoolStop);

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
        MessageStub&& _rmsg) noexcept
        : inner::Node<InnerLinkCount>(std::move(_rmsg))
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

    MessageBundle msgbundle;
    MessageId     msgid;
    ActorIdT      actid;
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
    solid_assert(cnt == _rlst.size());
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
    solid_assert(cnt == _rlst.size());
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
        DisconnectedFlag           = 128,
    };

    uint32_t               unique;
    uint16_t               persistent_connection_count;
    uint16_t               pending_connection_count;
    uint16_t               active_connection_count;
    uint16_t               stopping_connection_count;
    std::string            name; //because c_str() pointer is given to connection - name should allways be std::moved
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
        solid_assert(msgcache_inner_list.size() == msgvec.size());
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
        solid_assert(msgorder_inner_list.empty());
        solid_assert(msgasync_inner_list.empty());
        msgcache_inner_list.clear();
        msgorder_inner_list.clear();
        msgasync_inner_list.clear();
        msgvec.clear();
        msgvec.shrink_to_fit();
        flags               = 0;
        retry_connect_count = 0;
        connect_addr_vec.clear();
        solid_function_clear(on_event_fnc);
        solid_assert(msgorder_inner_list.check());
    }

    MessageId insertMessage(
        MessagePointerT&          _rmsgptr,
        const size_t              _msg_type_idx,
        MessageCompleteFunctionT& _rcomplete_fnc,
        const MessageFlagsT&      _flags,
        std::string&              _msg_url)
    {
        size_t idx;

        if (!msgcache_inner_list.empty()) {
            idx = msgcache_inner_list.frontIndex();
            msgcache_inner_list.popFront();
        } else {
            idx = msgvec.size();
            msgvec.push_back(MessageStub{});
        }

        MessageStub& rmsgstub(msgvec[idx]);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, _msg_url);

        //solid_assert(rmsgstub.msgbundle.message_ptr.get());

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

        solid_assert(msgorder_inner_list.check());

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

        solid_assert(msgorder_inner_list.check());

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

        solid_assert(!rmsgstub.msgbundle.message_ptr && rmsgstub.unique == _rmsgid.unique);

        rmsgstub.msgbundle = MessageBundle(_rmsgptr, _msg_type_idx, _flags, _rcomplete_fnc, _msg_url);

        msgorder_inner_list.pushFront(_rmsgid.index);

        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);

        if (Message::is_asynchronous(_flags)) {
            msgasync_inner_list.pushFront(_rmsgid.index);
            solid_dbg(logger, Info, "msgasync_inner_list " << msgasync_inner_list);
        }

        solid_assert(msgorder_inner_list.check());
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

        solid_assert(msgorder_inner_list.check());
    }

    void popFrontMessage()
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.popFront();

        solid_assert(msgorder_inner_list.check());
    }

    void eraseMessageOrder(const size_t _msg_idx)
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        solid_assert(msgorder_inner_list.check());
    }

    void eraseMessageOrderAsync(const size_t _msg_idx)
    {
        solid_dbg(logger, Info, "msgorder_inner_list " << msgorder_inner_list);
        msgorder_inner_list.erase(_msg_idx);
        const MessageStub& rmsgstub(msgvec[_msg_idx]);
        if (Message::is_asynchronous(rmsgstub.msgbundle.message_flags)) {
            solid_assert(msgasync_inner_list.contains(_msg_idx));
            msgasync_inner_list.erase(_msg_idx);
        }
        solid_assert(msgorder_inner_list.check());
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
        solid_assert(msgorder_inner_list.check());
    }

    void cacheMessageId(const MessageId& _rmsgid)
    {
        solid_assert(_rmsgid.index < msgvec.size());
        solid_assert(msgvec[_rmsgid.index].unique == _rmsgid.unique);
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

typedef std::deque<ConnectionPoolStub> ConnectionPoolDequeT;
typedef Stack<size_t>                  SizeStackT;

//-----------------------------------------------------------------------------

struct Service::Data {

    Data(Service& /*_rsvc*/)
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
    : BaseT(std::move(_force_shell), false)
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
void Service::doStart(Configuration&& _ucfg)
{
    Configuration cfg;
    SocketDevice  sd;

    cfg.reset(std::move(_ucfg));
    cfg.check();
    cfg.prepare(sd);

    Service::doStartWithoutAny(
        [this, &cfg, &sd]() {
            doFinalizeStart(std::move(cfg), std::move(sd));
        });
}
//-----------------------------------------------------------------------------
void Service::doStart()
{
    Service::doStartWithoutAny(
        [this]() {
            doFinalizeStart();
        });
}
//-----------------------------------------------------------------------------
void Service::doFinalizeStart(Configuration&& _ucfg, SocketDevice&& _usd)
{
    lock_guard<std::mutex> lock(impl_->mtx);
    if (_usd) {
        SocketAddress local_address;

        _usd.localAddress(local_address);

        ErrorConditionT error;
        ActorIdT        conuid = _ucfg.scheduler().startActor(make_dynamic<Listener>(_usd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        solid_check(!error, "Failed starting listener: " << error.message());

        impl_->config.reset(std::move(_ucfg));
        impl_->config.server.listener_port = local_address.port();
    } else {
        impl_->config.reset(std::move(_ucfg));
    }

    if (configuration().pools_mutex_count > impl_->mtxsarrcp) {
        delete[] impl_->pmtxarr; //TODO: delete
        impl_->pmtxarr   = new std::mutex[configuration().pools_mutex_count];
        impl_->mtxsarrcp = configuration().pools_mutex_count;
    }
}
//-----------------------------------------------------------------------------
void Service::doFinalizeStart()
{
    lock_guard<std::mutex> lock(impl_->mtx);
    SocketDevice           sd;

    impl_->config.prepare(sd);

    if (sd) {
        SocketAddress local_address;

        sd.localAddress(local_address);

        ErrorConditionT error;
        ActorIdT        conuid = configuration().scheduler().startActor(make_dynamic<Listener>(sd), *this, make_event(GenericEvents::Start), error);
        (void)conuid;

        solid_check(!error, "Failed starting listener: " << error.message());

        impl_->config.server.listener_port = local_address.port();
    }
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
        solid_dbg(logger, Info, "OnResolveF(addrvec of size " << _raddrvec.size() << ")");
        event.any() = ResolveMessage(std::move(_raddrvec));
        rm.notify(actuid, std::move(event));
    }
};

//-----------------------------------------------------------------------------

ErrorConditionT Service::doCreateConnectionPool(
    const char*           _recipient_url,
    RecipientId&          _rrecipient_id_out,
    PoolOnEventFunctionT& _event_fnc,
    const size_t          _persistent_connection_count)
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

    if (impl_->namemap.find(recipient_name) == impl_->namemap.end()) {
        //pool does not exist
        if (!impl_->conpoolcachestk.empty()) {
            pool_index = impl_->conpoolcachestk.top();
            impl_->conpoolcachestk.pop();
        } else {
            pool_index = this->doPushNewConnectionPool();
        }

        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

        rpool.name                        = recipient_name;
        rpool.persistent_connection_count = static_cast<uint16_t>(_persistent_connection_count);
        rpool.on_event_fnc                = std::move(_event_fnc);

        if (!doTryCreateNewConnectionForPool(pool_index, error)) {
            rpool.clear();
            impl_->conpoolcachestk.push(pool_index);
            return error;
        }

        impl_->namemap[rpool.name.c_str()] = pool_index;
        _rrecipient_id_out.poolid.index    = pool_index;
        _rrecipient_id_out.poolid.unique   = rpool.unique;
    } else {
        error = error_service_pool_exists;
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

    if (!running()) {
        solid_dbg(logger, Error, this << " service stopping");
        error = error_service_stopping;
        return error;
    }

    if (!_rmsgptr) {
        error = error_service_message_null;
        return error;
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
        solid_assert(_precipient_id_out == nullptr);
        //directly send the message to a connection actor
        return doSendMessageToConnection(
            _rrecipient_id_in,
            _rmsgptr,
            msg_type_idx,
            _rcomplete_fnc,
            _pmsgid_out,
            _flags,
            message_url);
    }

    if (recipient_name != nullptr) {
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
        error = error_service_pool_unknown;
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

    if (_precipient_id_out != nullptr) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_index, rpool.unique);
    }

    //At this point we can fetch the message from user's pointer
    //because from now on we can call complete on the message
    const MessageId msgid = rpool.pushBackMessage(_rmsgptr, msg_type_idx, _rcomplete_fnc, _flags, message_url);

    if (_pmsgid_out != nullptr) {

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
            solid_throw("Message Cancel connection not available");
        }
    }

    if (
        !success && Message::is_synchronous(_flags) &&
        //      rpool.main_connection_id.isValid() and
        rpool.isMainConnectionActive()) {
        success = manager().notify(
            rpool.main_connection_id,
            Connection::eventNewMessage());
        solid_assert(success);
    }

    if (!success && !Message::is_synchronous(_flags)) {
        success = doTryNotifyPoolWaitingConnection(pool_index);
    }

    if (!success) {
        doTryCreateNewConnectionForPool(pool_index, error);
        error.clear();
    }

    if (!success) {
        solid_dbg(logger, Info, this << " no connection notified about the new message");
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
    MessageFlagsT             _flags,
    std::string&              _msg_url)
{
    //d.mtx must be locked

    solid_dbg(logger, Verbose, this);

    if (!_rrecipient_id_in.isValidPool()) {
        solid_assert(false);
        return error_service_unknown_connection;
    }

    const size_t pool_index = static_cast<size_t>(_rrecipient_id_in.poolId().index);

    if (pool_index >= impl_->pooldq.size() || impl_->pooldq[pool_index].unique != _rrecipient_id_in.poolId().unique) {
        return error_service_unknown_connection;
    }

    _flags |= MessageFlagsE::OneShotSend;

    if (Message::is_response_part(_flags) || Message::is_response_last(_flags)) {
        _flags |= MessageFlagsE::Synchronous;
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
        msgid   = rpool.pushBackMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url);
        success = manager().notify(
            _rrecipient_id_in.connectionId(),
            Connection::eventNewMessage());
    } else {
        msgid   = rpool.insertMessage(_rmsgptr, _msg_type_idx, _rcomplete_fnc, _flags, _msg_url);
        success = manager().notify(
            _rrecipient_id_in.connectionId(),
            Connection::eventNewMessage(msgid));
    }

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

    if (!impl_->conpoolcachestk.empty()) {
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

    if (_precipient_id_out != nullptr) {
        _precipient_id_out->poolid = ConnectionPoolId(pool_index, rpool.unique);
    }

    if (_pmsgid_out != nullptr) {
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
    Connection&     _rcon,
    ActorIdT const& _ractuid,
    const size_t    _pool_index,
    const size_t    _msg_idx)
{

    solid_dbg(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    MessageStub&        rmsgstub                = rpool.msgvec[_msg_idx];
    const bool          message_is_asynchronous = Message::is_asynchronous(rmsgstub.msgbundle.message_flags);
    const bool          message_is_null         = !rmsgstub.msgbundle.message_ptr;
    bool                success                 = false;

    solid_assert(!Message::is_canceled(rmsgstub.msgbundle.message_flags));

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

    solid_dbg(logger, Verbose, this << " con = " << &_rcon);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);

    MessageStub& rmsgstub = rpool.msgvec[_rmsg_id.index];
    bool         success  = false;

    solid_assert(!Message::is_canceled(rmsgstub.msgbundle.message_flags));

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

    solid_dbg(logger, Verbose, this << " " << &_rconnection);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    ErrorConditionT error;

    solid_assert(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        error = error_service_pool_unknown;
        return error;
    }

    if (rpool.isMainConnectionStopping() && rpool.main_connection_id != _ractuid) {

        solid_dbg(logger, Info, this << ' ' << &_rconnection << " switch message main connection from " << rpool.main_connection_id << " to " << _ractuid);

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
    const bool connection_can_handle_synchronous_messages = _ractuid == rpool.main_connection_id;

    //We need to push as many messages as we can to the connection
    //in order to handle eficiently the situation with multiple small messages.

    if (!_rconnection.pendingMessageVector().empty()) {
        //connection has pending messages
        //fetch as many as we can
        size_t count = 0;

        for (const auto& rmsgid : _rconnection.pendingMessageVector()) {
            if (rmsgid.index < rpool.msgvec.size() && rmsgid.unique == rpool.msgvec[rmsgid.index].unique) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(_rconnection, _ractuid, pool_index, rmsgid);
                if (connection_may_handle_more_messages) {
                } else {
                    break;
                }
            } else {
                solid_assert(false);
            }
            ++count;
        }
        if (count != 0u) {
            _rconnection.pendingMessageVectorEraseFirst(count);
        }
    }
    if (_rconnection.isServer() || _rconnection.isActiveState()) {
        if (connection_can_handle_synchronous_messages) {
            //use the order inner queue
            while (!rpool.msgorder_inner_list.empty() && connection_may_handle_more_messages) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(
                    _rconnection,
                    _ractuid,
                    pool_index,
                    rpool.msgorder_inner_list.frontIndex());
            }

        } else {

            //use the async inner queue
            while (!rpool.msgasync_inner_list.empty() && connection_may_handle_more_messages) {
                connection_may_handle_more_messages = doTryPushMessageToConnection(
                    _rconnection,
                    _ractuid,
                    pool_index,
                    rpool.msgasync_inner_list.frontIndex());
            }
        }
        //a connection will either be in conn_waitingq
        //or it will call pollPoolForUpdates asap.
        //this is because we need to be able to notify connection about
        //pool force close imeditely
        if (!_rconnection.isInPoolWaitingQueue()) {
            rpool.conn_waitingq.push(_ractuid);
            _rconnection.setInPoolWaitingQueue();
        }
    } //if active state
    return error;
}
//-----------------------------------------------------------------------------
void Service::rejectNewPoolMessage(Connection const& _rconnection)
{

    solid_dbg(logger, Verbose, this);

    const size_t           pool_index = static_cast<size_t>(_rconnection.poolId().index);
    lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    solid_assert(rpool.unique == _rconnection.poolId().unique);
    if (rpool.unique != _rconnection.poolId().unique) {
        return;
    }

    doTryNotifyPoolWaitingConnection(pool_index);
}
//-----------------------------------------------------------------------------
bool Service::doTryNotifyPoolWaitingConnection(const size_t _pool_index)
{

    solid_dbg(logger, Verbose, this);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    bool                success = false;

    //we were not able to handle the message, try notify another connection
    while (!success && !rpool.conn_waitingq.empty()) {
        //a connection is waiting for something to send
        ActorIdT actuid = rpool.conn_waitingq.front();

        rpool.conn_waitingq.pop();

        success = manager().notify(
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

    if (!rpool.name.empty()) {
        impl_->namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;
    std::string     empty_str;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, 0, empty_str);
    (void)msgid;

    //notify all waiting connections about the new message
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

    if (!rpool.name.empty()) {
        impl_->namemap.erase(rpool.name.c_str());
    }

    MessagePointerT empty_msg_ptr;
    string          empty_str;

    const MessageId msgid = rpool.pushBackMessage(empty_msg_ptr, 0, _rcomplete_fnc, {MessageFlagsE::Synchronous}, empty_str);
    (void)msgid;

    //no reason to cancel all messages - they'll be handled on connection stop.
    //notify all waiting connections about the new message
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

            if (rmsgstub.actid.isValid()) { //message handled by a connection

                solid_dbg(logger, Verbose, this << " message " << _rmsg_id << " from pool " << pool_index << " is handled by connection " << rmsgstub.actid);

                solid_assert(!rmsgstub.msgbundle.message_ptr);

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
                    solid_throw("Lost message");
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
                    solid_throw("Message Cancel connection not available");
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
bool Service::fetchMessage(Connection& _rcon, ActorIdT const& _ractuid, MessageId const& _rmsg_id)
{

    solid_dbg(logger, Verbose, this);

    lock_guard<std::mutex> lock2(impl_->poolMutex(static_cast<size_t>(_rcon.poolId().index)));
    const size_t           pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

    if (
        _rmsg_id.index < rpool.msgvec.size() && rpool.msgvec[_rmsg_id.index].unique == _rmsg_id.unique) {
        return doTryPushMessageToConnection(_rcon, _ractuid, pool_index, _rmsg_id.index);
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
    ConnectionContext& _rconctx,
    ActorIdT const&    _ractuid,
    ulong&             _rseconds_to_wait,
    MessageId&         _rmsg_id,
    MessageBundle*     _pmsg_bundle,
    Event&             _revent_context,
    ErrorConditionT&   _rerror)
{

    solid_dbg(logger, Verbose, this);
    bool                retval;
    ConnectionPoolStub* ppool = nullptr;
    {
        Connection&            rcon(_rconctx.connection());
        lock_guard<std::mutex> lock(impl_->mtx);
        const size_t           pool_index = static_cast<size_t>(rcon.poolId().index);
        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

        _rseconds_to_wait = 0;

        if (_pmsg_bundle != nullptr) {
            _pmsg_bundle->clear();
        }

        solid_assert(rpool.unique == rcon.poolId().unique);
        if (rpool.unique != rcon.poolId().unique) {
            return false;
        }

        solid_dbg(logger, Info, this << ' ' << pool_index << " active_connection_count " << rpool.active_connection_count << " pending_connection_count " << rpool.pending_connection_count);

        bool was_disconnected = rpool.isDisconnected();

        if (!rpool.isMainConnection(_ractuid)) {
            retval = doNonMainConnectionStopping(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (!rpool.isLastConnection()) {
            retval = doMainConnectionStoppingNotLast(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isCleaningOneShotMessages()) {
            retval = doMainConnectionStoppingCleanOneShot(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isCleaningAllMessages()) {
            retval = doMainConnectionStoppingCleanAll(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (rpool.isRestarting() && running()) {
            retval = doMainConnectionRestarting(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else if (!rpool.isFastClosing() && !rpool.isServerSide() && running() /* && _rerror != error_connection_resolve*/) {
            retval = doMainConnectionStoppingPrepareCleanOneShot(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        } else {
            retval = doMainConnectionStoppingPrepareCleanAll(rcon, _ractuid, _rseconds_to_wait, _rmsg_id, _pmsg_bundle, _revent_context, _rerror);
        }
        if (!was_disconnected && rpool.isDisconnected() && !solid_function_empty(rpool.on_event_fnc)) {
            ppool = &rpool;
        }
    }
    if (ppool != nullptr) {
        //the call is safe because the current method is called from within a connection and
        // the connection pool entry is released when the last connection calls
        // Service::connectionStop
        ppool->on_event_fnc(_rconctx, pool_event_category.event(PoolEvents::PoolDisconnect), _rerror);
    }
    return retval;
}
//-----------------------------------------------------------------------------
bool Service::doNonMainConnectionStopping(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* _rmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count << " is active = " << _rcon.isActiveState());

    if (_rmsg_bundle == nullptr) {
        //cannot stop the connection right now - the connection has to clean its messages first
        return false;
    }

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        solid_assert(!_rcon.isServer());
        solid_assert(rpool.pending_connection_count >= 0);
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
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong&      _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_pmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    //it's the main connection but it is not the last one

    _rseconds_to_wait = 1;

    rpool.setMainConnectionStopping();
    rpool.resetMainConnectionActive();

    if (!rpool.isFastClosing()) {
        doFetchResendableMessagesFromConnection(_rcon);
    }

    return false;
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingCleanOneShot(
    Connection&    _rcon, ActorIdT const& /*_ractuid*/,
    ulong&         _rseconds_to_wait,
    MessageId&     _rmsg_id,
    MessageBundle* _pmsg_bundle,
    Event&         _revent_context,
    ErrorConditionT& /*_rerror*/
)
{

    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    size_t*      pmsgidx   = _revent_context.any().cast<size_t>();
    const size_t crtmsgidx = *pmsgidx;

    if (crtmsgidx != InvalidIndex()) {

        MessageStub& rmsgstub = rpool.msgvec[crtmsgidx];

        *pmsgidx = rpool.msgorder_inner_list.nextIndex(crtmsgidx);

        if (rpool.msgorder_inner_list.size() != 1) {
            solid_assert(rpool.msgorder_inner_list.contains(crtmsgidx));
        } else if (rpool.msgorder_inner_list.size() == 1) {
            solid_assert(rpool.msgorder_inner_list.frontIndex() == crtmsgidx);
        } else {
            solid_assert(false);
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
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingCleanAll(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId&     _rmsg_id,
    MessageBundle* _pmsg_bundle,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

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
            solid_assert(rpool.pending_connection_count >= 0);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;
        rpool.resetCleaningAllMessages();

        if (!rpool.name.empty() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
            impl_->namemap.erase(rpool.name.c_str());
            rpool.setClosing();
            solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");
        }

        return true; //TODO: maybe we should return false
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingPrepareCleanOneShot(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* _pmsg_bundle,
    Event&         _revent_context,
    ErrorConditionT& /*_rerror*/
)
{
    //the last connection
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    doFetchResendableMessagesFromConnection(_rcon);

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
            solid_assert(rpool.pending_connection_count >= 0);
            --rpool.pending_connection_count;
        }

        ++rpool.stopping_connection_count;

        if (!rpool.name.empty() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
            impl_->namemap.erase(rpool.name.c_str());
            rpool.setClosing();
            solid_dbg(logger, Verbose, this << " pool " << pool_index << " set closing");
        }
        return true; //the connection can call connectionStop asap
    }
    return false;
}
//-----------------------------------------------------------------------------
bool Service::doMainConnectionStoppingPrepareCleanAll(
    Connection& _rcon, ActorIdT const& /*_ractuid*/,
    ulong& /*_rseconds_to_wait*/,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    //the last connection - fast closing or server side
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    if (!rpool.name.empty() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
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
    Connection& _rcon, ActorIdT const& _ractuid,
    ulong& _rseconds_to_wait,
    MessageId& /*_rmsg_id*/,
    MessageBundle* /*_rmsg_bundle*/,
    Event& /*_revent_context*/,
    ErrorConditionT& /*_rerror*/
)
{
    const size_t        pool_index = static_cast<size_t>(_rcon.poolId().index);
    ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

    solid_dbg(logger, Info, this << ' ' << &_rcon << " pending = " << rpool.pending_connection_count << " active = " << rpool.active_connection_count);

    if (_rcon.isActiveState()) {
        --rpool.active_connection_count;
    } else {
        solid_assert(rpool.pending_connection_count >= 0);
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
            success = doTryCreateNewConnectionForPool(pool_index, error);
            tried   = true;
        }

        ++rpool.retry_connect_count;

        if (!success) {
            if (_rcon.isActiveState()) {
                ++rpool.active_connection_count;
            } else {
                solid_assert(!_rcon.isServer());
                ++rpool.pending_connection_count;
            }

            --rpool.stopping_connection_count;
            rpool.main_connection_id = _ractuid;

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
void Service::connectionStop(ConnectionContext& _rconctx)
{

    solid_dbg(logger, Info, this << ' ' << &_rconctx.connection());
    PoolOnEventFunctionT on_event_fnc;
    {
        const size_t            pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);
        unique_lock<std::mutex> lock2(impl_->poolMutex(pool_index));
        ConnectionPoolStub&     rpool(impl_->pooldq[pool_index]);

        solid_assert(rpool.unique == _rconctx.connection().poolId().unique);
        if (rpool.unique != _rconctx.connection().poolId().unique) {
            return;
        }

        --rpool.stopping_connection_count;
        rpool.resetRestarting();

        lock2.unlock();

        //do not call callback under lock
        if (!solid_function_empty(rpool.on_event_fnc) && _rconctx.connection().isConnected()) {
            rpool.on_event_fnc(_rconctx, pool_event_category.event(PoolEvents::ConnectionStop), ErrorConditionT());
        }

        configuration().relayEngine().stopConnection(_rconctx.relayId());
        configuration().connection_stop_fnc(_rconctx);

        //we might need to release the connectionpool entry - so we need the master lock
        lock_guard<std::mutex> lock(impl_->mtx);

        lock2.lock();

        if (rpool.hasNoConnection()) {

            solid_assert(rpool.msgorder_inner_list.empty());
            impl_->conpoolcachestk.push(pool_index);

            if (!rpool.name.empty() && !rpool.isClosing()) { //closing pools are already unregistered from namemap
                impl_->namemap.erase(rpool.name.c_str());
            }

            on_event_fnc = std::move(rpool.on_event_fnc);

            rpool.clear();
        }
    }
    if (!solid_function_empty(on_event_fnc)) {
        on_event_fnc(_rconctx, pool_event_category.event(PoolEvents::PoolStop), ErrorConditionT());
    }
}
//-----------------------------------------------------------------------------
bool Service::doTryCreateNewConnectionForPool(const size_t _pool_index, ErrorConditionT& _rerror)
{

    solid_dbg(logger, Verbose, this);

    ConnectionPoolStub& rpool(impl_->pooldq[_pool_index]);
    const bool          is_new_connection_needed = rpool.active_connection_count < rpool.persistent_connection_count || (rpool.hasAnyMessage() && rpool.conn_waitingq.size() < rpool.msgorder_inner_list.size());

    if (
        rpool.active_connection_count < configuration().pool_max_active_connection_count && rpool.pending_connection_count == 0 && is_new_connection_needed && running()) {

        solid_dbg(logger, Info, this << " try create new connection in pool " << rpool.active_connection_count << " pending connections " << rpool.pending_connection_count);

        DynamicPointer<aio::Actor> actptr(new_connection(configuration(), ConnectionPoolId(_pool_index, rpool.unique), rpool.name));
        ActorIdT                   conuid = impl_->config.scheduler().startActor(std::move(actptr), *this, make_event(GenericEvents::Start), _rerror);

        if (!_rerror) {

            solid_dbg(logger, Info, this << " Success starting Connection Pool actor: " << conuid.index << ',' << conuid.unique);

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

    if (!presolvemsg->addrvec.empty()) {
        lock_guard<std::mutex> lock(impl_->poolMutex(static_cast<size_t>(_rpoolid.index)));
        ConnectionPoolStub&    rpool(impl_->pooldq[static_cast<size_t>(_rpoolid.index)]);

        if (rpool.pending_connection_count < configuration().pool_max_pending_connection_count) {

            DynamicPointer<aio::Actor> actptr(new_connection(configuration(), _rpoolid, rpool.name));

            ActorIdT conuid = impl_->config.scheduler().startActor(std::move(actptr), *this, std::move(_revent), error);

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

    solid_assert(rpool.unique == _rpool_id.unique);

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
ErrorConditionT Service::activateConnection(ConnectionContext& _rconctx, ActorIdT const& _ractuid)
{

    solid_dbg(logger, Verbose, this);

    const size_t pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);

    ConnectionPoolStub* pconpool = nullptr;
    ErrorConditionT     error;
    {
        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

        solid_assert(rpool.unique == _rconctx.connection().poolId().unique);
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

            //return error;
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
                doTryCreateNewConnectionForPool(pool_index, err);
            }
        }

        rpool.resetDisconnected();

        if (!solid_function_empty(rpool.on_event_fnc)) {
            pconpool = &rpool;
        }
    }

    if (pconpool != nullptr) {
        pconpool->on_event_fnc(_rconctx, pool_event_category.event(PoolEvents::ConnectionActivate), error);
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
    bool                   from_cache = !impl_->conpoolcachestk.empty();

    if (from_cache) {
        pool_index = impl_->conpoolcachestk.top();
        impl_->conpoolcachestk.pop();
    } else {
        pool_index = this->doPushNewConnectionPool();
    }

    {
        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));

        ConnectionPoolStub& rpool(impl_->pooldq[pool_index]);

        DynamicPointer<aio::Actor> actptr(new_connection(configuration(), _rsd, ConnectionPoolId(pool_index, rpool.unique), rpool.name));

        solid::ErrorConditionT error;

        ActorIdT con_id = impl_->config.scheduler().startActor(
            std::move(actptr), *this, make_event(GenericEvents::Start), error);

        solid_dbg(logger, Info, this << " receive connection [" << con_id << "] error = " << error.message());

        if (error) {
            solid_assert(con_id.isInvalid());
            rpool.clear();
            impl_->conpoolcachestk.push(pool_index);
        } else {
            solid_assert(con_id.isValid());
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
    ConnectionPoolStub* ppool = nullptr;
    {
        const size_t           pool_index = static_cast<size_t>(_rconctx.connection().poolId().index);
        lock_guard<std::mutex> lock(impl_->mtx);
        lock_guard<std::mutex> lock2(impl_->poolMutex(pool_index));
        ConnectionPoolStub&    rpool(impl_->pooldq[pool_index]);

        solid_assert(rpool.unique == _rconctx.connection().poolId().unique);
        if (rpool.unique != _rconctx.connection().poolId().unique) {
            return;
        }

        if (!solid_function_empty(rpool.on_event_fnc)) {
            ppool = &rpool;
        }
    }

    //first call the more general function
    configuration().client.connection_start_fnc(_rconctx);

    if (ppool != nullptr) {
        //it should be safe using the pool right now because it is used
        // from within a pool's connection
        ppool->on_event_fnc(_rconctx, pool_event_category.event(PoolEvents::ConnectionStart), ErrorConditionT());
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
                solid_dbg(logger, Info, "add resolved endpoint: " << addrvec.back() << ':' << addrvec.back().port());
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
        if (svc_name[0] == 0) {
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
} //namespace mprpc
} //namespace frame
} //namespace solid
