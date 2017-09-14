// solid/frame/ipc/src/mpipcrelayengine.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mpipc/mpipcrelayengine.hpp"
#include "mpipcutility.hpp"
#include "solid/system/debug.hpp"
#include "solid/utility/string.hpp"
#include <deque>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>

using namespace std;

namespace solid {
namespace frame {
namespace mpipc {
//-----------------------------------------------------------------------------
namespace {

enum struct ConnectionStateE {
    Active,
};

enum struct InnerListE {
    Recv,
    Send,
    Count //add above
};

constexpr const size_t inner_list_count = static_cast<size_t>(InnerListE::Count);

constexpr const size_t cast(const InnerListE _l)
{
    return static_cast<size_t>(_l);
}

struct InnerList {
    size_t front_;
    size_t back_;

    InnerList(const size_t _front = InvalidIndex(), const size_t _back = InvalidIndex())
        : front_(_front)
        , back_(_back)
    {
    }

    void clear()
    {
        front_ = InvalidIndex();
        back_  = InvalidIndex();
    }

    bool empty() const
    {
        return front_ == InvalidIndex();
    }
};

struct InnerNode {
    size_t prev_;
    size_t next_;

    InnerNode(const size_t _prev = InvalidIndex(), const size_t _next = InvalidIndex())
        : prev_(_prev)
        , next_(_next)
    {
    }
    void clear()
    {
        prev_ = InvalidIndex();
        next_ = InvalidIndex();
    }
};

struct ConnectionStub {
    uint32_t   unique_;
    ObjectIdT  id_;
    string     name_;
    RelayData* pdone_relay_data_top_;
    InnerList  inner_lists_[inner_list_count];

    ConnectionStub()
        : unique_(0)
        , pdone_relay_data_top_(nullptr)
    {
    }
    ConnectionStub(std::string&& _uname)
        : unique_(0)
        , name_(std::move(_uname))
        , pdone_relay_data_top_(nullptr)
    {
    }

    void clear()
    {
        ++unique_;
        id_.clear();
        name_.clear();
        pdone_relay_data_top_ = nullptr;
        inner_lists_[cast(InnerListE::Recv)].clear();
        inner_lists_[cast(InnerListE::Send)].clear();
    }
};

/*
NOTE:
    Message turning points:
    * sender connection closes
        * if the message was not completely received, receiver connection must send message cancel
        * if the message is waiting for response, receiver connection must send CancelRequest
        * -
        * 
    * receiver connection closes
        * if the message was not completely received, sender connection must send CancelRequest and discard any incomming data
        * if message is waiting for response, sender connection must send CancelRequest
        * the connection must call RelayEngine::doCompleteClose on all relayed messages in its writer
        *   then RelayEngine::connectionStop is called.
        - Ideas:
            - on doCompleteClose can add _prelay_data back to message list and add the message to the receiver connection message list
            - on connectionStop
    * sender connection receives a message canceled command
        * 
    * receiver connection receives message cancel request command which should be forwarder up to the initial sender of the message
*/

enum struct MessageStateE {
    Relay,
    WaitResponse,
    Cancel
};

struct MessageStub {
    MessageStateE state_;
    RelayData*    pfront_;
    RelayData*    pback_;
    uint32_t      unique_;
    ObjectIdT     sender_con_id_;
    ObjectIdT     receiver_con_id_;
    MessageId     receiver_msg_id_;
    InnerNode     inner_nodes_[inner_list_count];
    MessageHeader header_;

    MessageStub()
        : state_(MessageStateE::Relay)
        , pfront_(nullptr)
        , pback_(nullptr)
        , unique_(0)
    {
    }
    void clear()
    {
        state_  = MessageStateE::Relay;
        pfront_ = nullptr;
        pback_  = nullptr;
        ++unique_;
        sender_con_id_.clear();
        receiver_con_id_.clear();
        receiver_msg_id_.clear();
        inner_nodes_[cast(InnerListE::Recv)].clear();
        inner_nodes_[cast(InnerListE::Send)].clear();
    }

    void push(RelayData* _prd)
    {
        if (pback_) {
            pback_->pnext_ = _prd;
            pback_         = _prd;
        } else {
            pfront_ = pback_ = _prd;
            pback_->pnext_   = nullptr;
        }
        pback_->pmessage_header_ = &header_;
    }
};

using MessageDequeT    = std::deque<MessageStub>;
using RelayDataDequeT  = std::deque<RelayData>;
using RelayDataStackT  = std::stack<RelayData*>;
using SizeTStackT      = std::stack<size_t>;
using ConnectionDequeT = std::deque<ConnectionStub>;
using ConnectionMapT   = std::unordered_map<const char*, size_t, CStringHash, CStringEqual>;
using ConnectionIdMapT = std::unordered_map<size_t, size_t>;
} //namespace

struct RelayEngine::Data {
    mutex            mtx_;
    MessageDequeT    msg_dq_;
    RelayDataDequeT  reldata_dq_;
    RelayDataStackT  reldata_cache_;
    SizeTStackT      msg_cache_;
    ConnectionDequeT con_dq_;
    ConnectionMapT   con_umap_;
    ConnectionIdMapT con_id_umap_;
    SizeTStackT      con_cache_;

    RelayData* createRelayData(RelayData&& _urd)
    {
        RelayData* prd = nullptr;
        if (reldata_cache_.size()) {
            prd = reldata_cache_.top();
            reldata_cache_.pop();
            *prd = std::move(_urd);
        } else {
            reldata_dq_.emplace_back(std::move(_urd));
            prd = &reldata_dq_.back();
        }
        return prd;
    }
    void freeRelayData(RelayData*& _prd)
    {
        reldata_cache_.push(_prd);
        _prd = nullptr;
    }

    size_t createMessage()
    {
        size_t idx;
        if (msg_cache_.size()) {
            idx = msg_cache_.top();
            msg_cache_.pop();
        } else {
            idx = msg_dq_.size();
            msg_dq_.emplace_back();
        }
        return idx;
    }
    void freeMessage(const size_t _idx)
    {
        msg_cache_.push(_idx);
    }

    bool innerListEmpty(const InnerListE _l, const size_t _conidx) const
    {
        return con_dq_[_conidx].inner_lists_[cast(_l)].empty();
    }

    size_t innerListFrontIndex(const InnerListE _l, const size_t _conidx) const
    {
        return con_dq_[_conidx].inner_lists_[cast(_l)].front_;
    }

    size_t innerListBackIndex(const InnerListE _l, const size_t _conidx) const
    {
        return con_dq_[_conidx].inner_lists_[cast(_l)].back_;
    }

    void innerListPushBack(const InnerListE _l, const size_t _conidx, const size_t _msgidx)
    {
        InnerList& rl = con_dq_[_conidx].inner_lists_[cast(_l)];
        InnerNode& rn = msg_dq_[_msgidx].inner_nodes_[cast(_l)];
        if (not rl.empty()) {
            rn.prev_                                       = rl.back_;
            rn.next_                                       = InvalidIndex();
            msg_dq_[rl.back_].inner_nodes_[cast(_l)].next_ = _msgidx;
            rl.back_                                       = _msgidx;
        } else {
            rn.clear();
            rl.front_ = rl.back_ = _msgidx;
        }
    }

    void innerListPushFront(const InnerListE _l, const size_t _conidx, const size_t _msgidx)
    {
        InnerList& rl = con_dq_[_conidx].inner_lists_[cast(_l)];
        InnerNode& rn = msg_dq_[_msgidx].inner_nodes_[cast(_l)];
        if (not rl.empty()) {
            rn.next_                                        = rl.front_;
            rn.prev_                                        = InvalidIndex();
            msg_dq_[rl.front_].inner_nodes_[cast(_l)].prev_ = _msgidx;
            rl.front_                                       = _msgidx;
        } else {
            rn.clear();
            rl.front_ = rl.back_ = _msgidx;
        }
    }

    void innerListErase(const InnerListE _l, const size_t _conidx, const size_t _msgidx)
    {
        InnerList& rl = con_dq_[_conidx].inner_lists_[cast(_l)];
        InnerNode& rn = msg_dq_[_msgidx].inner_nodes_[cast(_l)];

        if (rn.prev_ != InvalidIndex()) {
            msg_dq_[rn.prev_].inner_nodes_[cast(_l)].next_ = rn.next_;
        } else {
            //first in the list
            rl.front_ = rn.next_;
        }
        if (rn.next_ != InvalidIndex()) {
            msg_dq_[rn.next_].inner_nodes_[cast(_l)].prev_ = rn.prev_;
        } else {
            //first in the list
            rl.back_ = rn.prev_;
        }
    }

    size_t innerListPopFront(const InnerListE _l, const size_t _conidx)
    {
        size_t idx = innerListFrontIndex(_l, _conidx);
        innerListErase(_l, _conidx, idx);
        return idx;
    }
};
//-----------------------------------------------------------------------------
RelayEngine::RelayEngine()
    : impl_(make_pimpl<Data>())
{
}
//-----------------------------------------------------------------------------
RelayEngine::~RelayEngine()
{
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionStop(Service& _rsvc, const ObjectIdT& _rconuid)
{
    unique_lock<mutex> lock(impl_->mtx_);

    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        if (rcon.id_.unique == _rconuid.unique) {

            RelayData* prd = rcon.pdone_relay_data_top_;

            idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " con_idx = " << con_idx);

            while (prd) {
                RelayData* ptmprd = prd->pnext_;
                prd->clear();
                impl_->reldata_cache_.push(prd);
                prd = ptmprd;
            }

            if (not rcon.name_.empty()) {
                impl_->con_umap_.erase(rcon.name_.c_str());
            }

            impl_->con_id_umap_.erase(rcon.id_.index);

            rcon.clear();
            impl_->con_cache_.push(_rconuid.index);
        }
    }

    edbgx(Debug::mpipc, "connection not registered: " << _rconuid);
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(Service& _rsvc, const ObjectIdT& _rconuid, std::string&& _uname)
{
    to_lower(_uname);
    unique_lock<mutex> lock(impl_->mtx_);

    size_t con_idx = InvalidIndex();
    {
        const auto it = impl_->con_id_umap_.find(_rconuid.index);

        if (it != impl_->con_id_umap_.end()) {
            const size_t    tmp_con_idx = it->second;
            ConnectionStub& rcon        = impl_->con_dq_[tmp_con_idx];

            if (rcon.id_.unique == _rconuid.unique) {
                con_idx = tmp_con_idx;
            }
        }
    }
    if (not _uname.empty()) {
        const auto it = impl_->con_umap_.find(_uname.c_str());

        if (it != impl_->con_umap_.end()) {
            //TODO: see "CONFLICT scenario" below
            SOLID_ASSERT(con_idx == InvalidIndex() or con_idx == it->second);
            con_idx = it->second;
        } else if (con_idx != InvalidIndex()) {
            impl_->con_dq_[con_idx].name_                           = std::move(_uname);
            impl_->con_umap_[impl_->con_dq_[con_idx].name_.c_str()] = con_idx;
        } else if (impl_->con_cache_.size()) {
            con_idx = impl_->con_cache_.top();
            impl_->con_cache_.pop();
            impl_->con_dq_[con_idx].name_                           = std::move(_uname);
            impl_->con_umap_[impl_->con_dq_[con_idx].name_.c_str()] = con_idx;
        } else {
            con_idx = impl_->con_dq_.size();
            impl_->con_dq_.emplace_back(std::move(_uname));
            impl_->con_umap_[impl_->con_dq_.back().name_.c_str()] = con_idx;
        }
    } else {
        //TODO:
        con_idx = doRegisterConnection(_rconuid);
        SOLID_ASSERT(false);
    }

    impl_->con_id_umap_[_rconuid.index] = con_idx;

    ConnectionStub& rcon = impl_->con_dq_[con_idx];
    rcon.id_             = _rconuid;

    if (not impl_->innerListEmpty(InnerListE::Recv, con_idx)) {
        if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::NewData)) {
            //TODO:
            SOLID_ASSERT(false);
        } else {
            idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " notified about new relay data");
        }
    }

    idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " con_idx = " << con_idx);
}
//-----------------------------------------------------------------------------
size_t RelayEngine::doRegisterConnection(const ObjectIdT& _rconuid)
{
    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        if (rcon.id_.unique == _rconuid.unique) {
            vdbgx(Debug::mpipc, con_idx);
            return con_idx;
        }
    }

    size_t con_idx = InvalidIndex();

    if (impl_->con_cache_.size()) {
        con_idx = impl_->con_cache_.top();
        impl_->con_cache_.pop();
        impl_->con_dq_[con_idx].name_.clear();
    } else {
        con_idx = impl_->con_dq_.size();
        impl_->con_dq_.emplace_back();
    }

    impl_->con_dq_[con_idx].id_         = _rconuid;
    impl_->con_id_umap_[_rconuid.index] = con_idx;
    vdbgx(Debug::mpipc, con_idx);
    return con_idx;
}
//-----------------------------------------------------------------------------
size_t RelayEngine::doRegisterConnection(std::string&& _uname)
{
    size_t     con_idx = InvalidIndex();
    const auto it      = impl_->con_umap_.find(_uname.c_str());

    if (it != impl_->con_umap_.end()) {
        con_idx = it->second;
    } else if (impl_->con_cache_.size()) {
        con_idx = impl_->con_cache_.top();
        impl_->con_cache_.pop();
        impl_->con_dq_[con_idx].name_                           = std::move(_uname);
        impl_->con_umap_[impl_->con_dq_[con_idx].name_.c_str()] = con_idx;
    } else {
        con_idx = impl_->con_dq_.size();
        impl_->con_dq_.emplace_back(std::move(_uname));
        impl_->con_umap_[impl_->con_dq_.back().name_.c_str()] = con_idx;
    }
    idbgx(Debug::mpipc, "name = " << impl_->con_dq_[con_idx].name_ << " con_idx = " << con_idx);
    return con_idx;
}
//-----------------------------------------------------------------------------
bool RelayEngine::doRelayStart(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    MessageId&       _rrelay_id,
    ErrorConditionT& _rerror)
{
    size_t msgidx;

    unique_lock<mutex> lock(impl_->mtx_);
    SOLID_ASSERT(_rrelay_id.isInvalid());

    {
        to_lower(_rmsghdr.url_);

        size_t     sender_con_idx = InvalidIndex();
        const auto it             = impl_->con_id_umap_.find(_rconuid.index);
        if (it == impl_->con_id_umap_.end()) {
            //TODO: maybe set _rerror
            wdbgx(Debug::mpipc, "relay must be called only on registered connections");
            //CONFLICT scenario:
            //con B sends a message to not existing connection "A"
            //  a connection stub is crated for "A" with idA1
            //unreginstered connection A, sends a message to con "B"
            //  an unnamed connection stub must be created for A with idA2 associated to conIdA
            //connection A wants to register with name "A"
            //  conIdA is associated to stub idA2 and "A" is associated idA1 (CONFLICT)

            //return false;
            sender_con_idx = doRegisterConnection(_rconuid);
        } else {
            sender_con_idx = it->second;
        }
        msgidx            = impl_->createMessage();
        MessageStub& rmsg = impl_->msg_dq_[msgidx];

        rmsg.inner_nodes_[cast(InnerListE::Recv)].clear();
        rmsg.inner_nodes_[cast(InnerListE::Send)].clear();
        rmsg.header_ = std::move(_rmsghdr);

        const size_t    rcv_con_idx = doRegisterConnection(std::move(_rmsghdr.url_));
        ConnectionStub& rrcv_con    = impl_->con_dq_[rcv_con_idx];

        //also hold the in-engine connection id into msg
        rmsg.sender_con_id_   = ObjectIdT(sender_con_idx, impl_->con_dq_[sender_con_idx].unique_);
        rmsg.receiver_con_id_ = ObjectIdT(rcv_con_idx, rrcv_con.unique_);

        impl_->innerListPushBack(InnerListE::Send, sender_con_idx, msgidx); //register message onto sender connection
    }

    MessageStub& rmsg = impl_->msg_dq_[msgidx];

    idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " size = " << _rrelmsg.data_size_ << " receiver_con_idx " << rmsg.receiver_con_id_.index << " sender_con_idx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_);

    SOLID_ASSERT(rmsg.pfront_ == nullptr);

    rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

    const bool recv_list_empty = impl_->innerListEmpty(InnerListE::Recv, rmsg.receiver_con_id_.index);

    impl_->innerListPushBack(InnerListE::Recv, rmsg.receiver_con_id_.index, msgidx);

    if (recv_list_empty) {
        ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
        if (rcon.id_.isValid() and not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::NewData)) {
            //TODO:
            SOLID_ASSERT(false);
        }
    }

    return true;
}
//-----------------------------------------------------------------------------
bool RelayEngine::doRelayResponse(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    unique_lock<mutex> lock(impl_->mtx_);
    SOLID_ASSERT(_rrelay_id.isValid());

    if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
        const size_t msgidx            = _rrelay_id.index;
        MessageStub& rmsg              = impl_->msg_dq_[msgidx];
        RequestId    sender_request_id = rmsg.header_.sender_request_id_;
        idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_con_idx " << rmsg.receiver_con_id_ << " sender_con_idx " << rmsg.sender_con_id_ << " is_last = " << _rrelmsg.is_last_);

        impl_->innerListErase(InnerListE::Send, rmsg.sender_con_id_.index, msgidx);

        std::swap(rmsg.receiver_con_id_, rmsg.sender_con_id_);

        impl_->innerListPushBack(InnerListE::Send, rmsg.sender_con_id_.index, msgidx);

        rmsg.receiver_msg_id_.clear();

        SOLID_ASSERT(rmsg.pfront_ == nullptr);
        rmsg.header_ = std::move(_rmsghdr);
        //set the proper recipient_request_id_
        rmsg.header_.sender_request_id_ = sender_request_id;

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        const bool recv_list_empty = impl_->innerListEmpty(InnerListE::Recv, rmsg.receiver_con_id_.index);

        impl_->innerListPushBack(InnerListE::Recv, rmsg.receiver_con_id_.index, msgidx);

        if (recv_list_empty) {
            ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
            if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::NewData)) {
                //TODO:
                SOLID_ASSERT(false);
            }
        }

        return true;
    } else {
        edbgx(Debug::mpipc, "message not found");
        return false;
    }
}
//-----------------------------------------------------------------------------
bool RelayEngine::doRelay(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    RelayData&&      _rrelmsg,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    unique_lock<mutex> lock(impl_->mtx_);
    SOLID_ASSERT(_rrelay_id.isValid());

    if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
        const size_t msgidx                        = _rrelay_id.index;
        MessageStub& rmsg                          = impl_->msg_dq_[msgidx];
        bool         is_msg_relay_data_queue_empty = (rmsg.pfront_ == nullptr);

        idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_con_idx " << rmsg.receiver_con_id_.index << " sender_con_idx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_ << " is_msg_relay_data_queue_empty = " << is_msg_relay_data_queue_empty);

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        if (is_msg_relay_data_queue_empty) {

            const bool recv_list_empty = impl_->innerListEmpty(InnerListE::Recv, rmsg.receiver_con_id_.index);

            impl_->innerListPushBack(InnerListE::Recv, rmsg.receiver_con_id_.index, msgidx);

            if (recv_list_empty) {
                ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
                if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::NewData)) {
                    //TODO:
                    SOLID_ASSERT(false);
                }
            }
        }
        return true;
    } else {
        edbgx(Debug::mpipc, _rconuid << " message not found " << _rrelay_id);
        return false;
    }
}
//-----------------------------------------------------------------------------
void RelayEngine::doPollNew(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);
    //TODO: optimize the search
    const auto it = impl_->con_id_umap_.find(_rconuid.index);
    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);
        SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);

        bool   can_retry   = true;
        size_t sentinel    = impl_->innerListBackIndex(InnerListE::Recv, con_idx);
        size_t crt_msg_idx = impl_->innerListFrontIndex(InnerListE::Recv, con_idx);

        while (can_retry and crt_msg_idx != InvalidIndex()) {
            MessageStub& rmsg  = impl_->msg_dq_[crt_msg_idx];
            RelayData*   pnext = rmsg.pfront_->pnext_;

            edbgx(Debug::mpipc, _rconuid << " msgidx = " << crt_msg_idx << " pnext = " << pnext);

            if (_try_push_fnc(rmsg.pfront_, MessageId(crt_msg_idx, rmsg.unique_), rmsg.receiver_msg_id_, can_retry)) {
                //relay data accepted
                rmsg.pfront_ = pnext;

                if (rmsg.pfront_ == nullptr) {
                    rmsg.pback_ = nullptr;
                }
                impl_->innerListPopFront(InnerListE::Recv, con_idx);
                crt_msg_idx = impl_->innerListFrontIndex(InnerListE::Recv, con_idx);
                //drop the message from the connection queue
                vdbgx(Debug::mpipc, _rconuid << " " << crt_msg_idx);
            } else if (can_retry) {
                //relay data not accepted but we can try with other message
                //reschedule the message onto the connection's message queue

                impl_->innerListPushBack(InnerListE::Recv, con_idx, impl_->innerListPopFront(InnerListE::Recv, con_idx));
                crt_msg_idx = impl_->innerListFrontIndex(InnerListE::Recv, con_idx);
                vdbgx(Debug::mpipc, _rconuid << " " << crt_msg_idx);
                if (crt_msg_idx == sentinel) {
                    break;
                }
            } else {
                break;
            }
        }

        _rmore = not impl_->innerListEmpty(InnerListE::Recv, con_idx);
        ;
    } else {
        edbgx(Debug::mpipc, _rconuid << " connection not found");
    }
}
//-----------------------------------------------------------------------------
void RelayEngine::doPollDone(const ObjectIdT& _rconuid, DoneFunctionT& _done_fnc)
{
    unique_lock<mutex> lock(impl_->mtx_);

    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        edbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);
        SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);

        RelayData* prd = rcon.pdone_relay_data_top_;

        while (prd) {
            _done_fnc(prd->bufptr_);
            RelayData* ptmprd = prd->pnext_;
            prd->clear();
            impl_->reldata_cache_.push(prd);
            prd = ptmprd;
        }

        rcon.pdone_relay_data_top_ = nullptr;
    } else {
        edbgx(Debug::mpipc, _rconuid << " connection not found");
    }
}
//-----------------------------------------------------------------------------
//called by receiving connection after using the relay_data
void RelayEngine::doComplete(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    RelayData*       _prelay_data,
    MessageId const& _rengine_msg_id,
    bool&            _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);

    idbgx(Debug::mpipc, _rconuid << " try complete msg " << _rengine_msg_id);

    if (_rengine_msg_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rengine_msg_id.index].unique_ == _rengine_msg_id.unique) {
        MessageStub& rmsg = impl_->msg_dq_[_rengine_msg_id.index];

        if (rmsg.pfront_ != nullptr) {

            SOLID_ASSERT(rmsg.receiver_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.receiver_con_id_.index].unique_ == rmsg.receiver_con_id_.unique);
            ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];

            SOLID_ASSERT(rcon.id_ == _rconuid);

            idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

            const bool recv_list_empty = impl_->innerListEmpty(InnerListE::Recv, rmsg.receiver_con_id_.index);

            impl_->innerListPushBack(InnerListE::Recv, rmsg.receiver_con_id_.index, _rengine_msg_id.index);

            if (recv_list_empty) {
                ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
                if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::NewData)) {
                    //TODO:
                    SOLID_ASSERT(false);
                }
            }
        }

        if (rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique) {
            ConnectionStub& rcon = impl_->con_dq_[rmsg.sender_con_id_.index];

            idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

            bool was_pdone_relay_data_stack_empty = rcon.pdone_relay_data_top_ == nullptr;

            if (_prelay_data) {
                _prelay_data->pnext_       = rcon.pdone_relay_data_top_;
                rcon.pdone_relay_data_top_ = _prelay_data;

                if (was_pdone_relay_data_stack_empty) {
                    if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::DoneData)) {
                        //TODO:
                        SOLID_ASSERT(false);
                    }
                }

                if (_prelay_data->is_last_ and not Message::is_waiting_response(_prelay_data->pmessage_header_->flags_)) {
                    SOLID_ASSERT(rmsg.pfront_ == nullptr);
                    impl_->innerListErase(InnerListE::Send, rmsg.sender_con_id_.index, _rengine_msg_id.index);
                    rmsg.clear();
                    idbgx(Debug::mpipc, _rconuid << " clear msg " << _rengine_msg_id);
                    impl_->msg_cache_.push(_rengine_msg_id.index);
                }
            } else {
                impl_->innerListErase(InnerListE::Send, rmsg.sender_con_id_.index, _rengine_msg_id.index);
                rmsg.clear();
                impl_->msg_cache_.push(_rengine_msg_id.index);
            }
        } else {
            edbgx(Debug::mpipc, _rconuid << " sender connection not found " << rmsg.sender_con_id_);
        }
    } else {
        _prelay_data->clear();
        impl_->reldata_cache_.push(_prelay_data);
        edbgx(Debug::mpipc, _rconuid << " message not found " << _rengine_msg_id);
    }
}
//-----------------------------------------------------------------------------
// called by the receiver connection (the one forwarding the relay data) on close
// _prelay_data must somehow reach the sending connection
//
void RelayEngine::doCompleteClose(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    RelayData*       _prelay_data,
    MessageId const& _rengine_msg_id)
{
    unique_lock<mutex> lock(impl_->mtx_);

    idbgx(Debug::mpipc, _rconuid << " try complete cancel msg " << _rengine_msg_id);

    if (_rengine_msg_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rengine_msg_id.index].unique_ == _rengine_msg_id.unique) {
        MessageStub& rmsg = impl_->msg_dq_[_rengine_msg_id.index];
        SOLID_ASSERT(rmsg.receiver_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.receiver_con_id_.index].unique_ == rmsg.receiver_con_id_.unique);

        if (rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique) {
            ConnectionStub& rcon = impl_->con_dq_[rmsg.sender_con_id_.index];

            idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

            bool was_pdone_relay_data_stack_empty = rcon.pdone_relay_data_top_ == nullptr;

            if (_prelay_data) {
                _prelay_data->pnext_       = rcon.pdone_relay_data_top_;
                rcon.pdone_relay_data_top_ = _prelay_data;

                if (was_pdone_relay_data_stack_empty) {
                    if (not notify_connection(_rsvc, rcon.id_, RelayEngineNotification::DoneData)) {
                        //TODO:
                        SOLID_ASSERT(false);
                    }
                }

            } else {
                //TODO:
            }
        } else {
            edbgx(Debug::mpipc, _rconuid << " sender connection not found " << rmsg.sender_con_id_);
        }
    }
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
