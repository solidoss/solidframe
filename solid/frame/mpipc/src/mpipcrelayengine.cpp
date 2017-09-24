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
#include "solid/utility/innerlist.hpp"
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

enum {
    InnerLinkRecv,
    InnerLinkSend,
    InnerLinkCount //add above
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
    Cache,
    Relay,
    WaitResponse,
    RecvCancel,
    SendCancel,
};

struct MessageStub : inner::Node<InnerLinkCount> {
    MessageStateE state_;
    RelayData*    pfront_;
    RelayData*    pback_;
    uint32_t      unique_;
    ObjectIdT     sender_con_id_;
    ObjectIdT     receiver_con_id_;
    MessageId     receiver_msg_id_;
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
        SOLID_ASSERT(pfront_ == nullptr and pback_ == nullptr);
        state_  = MessageStateE::Cache;
        pfront_ = nullptr;
        pback_  = nullptr;
        ++unique_;
        sender_con_id_.clear();
        receiver_con_id_.clear();
        receiver_msg_id_.clear();
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

    RelayData* pop()
    {
        RelayData* p = nullptr;
        if (pfront_) {
            p       = pfront_;
            pfront_ = p->pnext_;
            if (pfront_ == nullptr) {
                pback_ = nullptr;
            }
            p->pnext_ = nullptr;
        }
        return p;
    }

    bool isCanceled() const
    {
        return state_ == MessageStateE::RecvCancel or state_ == MessageStateE::SendCancel;
    }

    bool isActive() const
    {
        return pback_ != nullptr or state_ == MessageStateE::SendCancel;
    }
};

using MessageDequeT  = std::deque<MessageStub>;
using SendInnerListT = inner::List<MessageDequeT, InnerLinkSend>;
using RecvInnerListT = inner::List<MessageDequeT, InnerLinkRecv>;

std::ostream& operator<<(std::ostream& _ros, const SendInnerListT& _rlst)
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

std::ostream& operator<<(std::ostream& _ros, const RecvInnerListT& _rlst)
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

struct ConnectionStub {
    uint32_t       unique_;
    ObjectIdT      id_;
    string         name_;
    RelayData*     pdone_relay_data_top_;
    SendInnerListT send_msg_list_;
    RecvInnerListT recv_msg_list_;

    ConnectionStub(MessageDequeT& _rmsg_dq)
        : unique_(0)
        , pdone_relay_data_top_(nullptr)
        , send_msg_list_(_rmsg_dq)
        , recv_msg_list_(_rmsg_dq)
    {
    }

    ConnectionStub(MessageDequeT& _rmsg_dq, std::string&& _uname)
        : unique_(0)
        , name_(std::move(_uname))
        , pdone_relay_data_top_(nullptr)
        , send_msg_list_(_rmsg_dq)
        , recv_msg_list_(_rmsg_dq)
    {
    }

    void clear()
    {
        ++unique_;
        id_.clear();
        name_.clear();
        pdone_relay_data_top_ = nullptr;
        SOLID_ASSERT(send_msg_list_.empty());
        SOLID_ASSERT(recv_msg_list_.empty());
    }
};

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
    RelayData*       prelay_data_cache_top_;
    SendInnerListT   msg_cache_inner_list_;
    ConnectionDequeT con_dq_;
    ConnectionMapT   con_umap_;
    ConnectionIdMapT con_id_umap_;
    SizeTStackT      con_cache_;

    Data()
        : prelay_data_cache_top_(nullptr)
        , msg_cache_inner_list_(msg_dq_)
    {
    }

    RelayData* createRelayData(RelayData&& _urd)
    {
        RelayData* prd = nullptr;
        if (prelay_data_cache_top_) {
            prd                    = prelay_data_cache_top_;
            prelay_data_cache_top_ = prelay_data_cache_top_->pnext_;
            *prd                   = std::move(_urd);
        } else {
            reldata_dq_.emplace_back(std::move(_urd));
            prd = &reldata_dq_.back();
        }
        return prd;
    }
    void freeRelayData(RelayData*& _prd)
    {
        _prd->pnext_           = prelay_data_cache_top_;
        prelay_data_cache_top_ = _prd;
        _prd                   = nullptr;
    }

    size_t createMessage()
    {
        size_t idx;

        if (msg_cache_inner_list_.size()) {
            return msg_cache_inner_list_.popBack();
        } else {
            idx = msg_dq_.size();
            msg_dq_.emplace_back();
        }
        return idx;
    }
    void freeMessage(const size_t _idx)
    {
        msg_cache_inner_list_.pushBack(_idx);
    }
};
//-----------------------------------------------------------------------------
RelayEngine::RelayEngine()
    : impl_(make_pimpl<Data>())
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
RelayEngine::~RelayEngine()
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionStop(Service& _rsvc, const ObjectIdT& _rconuid)
{
    unique_lock<mutex> lock(impl_->mtx_);
    size_t             conidx;
    {
        const auto it = impl_->con_id_umap_.find(_rconuid.index);

        if (it == impl_->con_id_umap_.end() or impl_->con_dq_[it->second].id_.unique != _rconuid.unique) {
            edbgx(Debug::mpipc, "connection not registered: " << _rconuid);
            return;
        } else {
            conidx = it->second;
        }
    }

    ConnectionStub& rcon = impl_->con_dq_[conidx];
    {
        while (rcon.recv_msg_list_.size()) {
            MessageStub& rmsg       = rcon.recv_msg_list_.front();
            const size_t msgidx     = rcon.recv_msg_list_.popFront();
            const size_t snd_conidx = rmsg.sender_con_id_.index;

            rmsg.receiver_con_id_.clear(); //unlink from the receiver connection

            if (rmsg.sender_con_id_.isValid()) {
                switch (rmsg.state_) {
                case MessageStateE::Relay:
                case MessageStateE::WaitResponse:
                    rmsg.state_ = MessageStateE::RecvCancel;

                    SOLID_ASSERT(snd_conidx < impl_->con_dq_.size() and impl_->con_dq_[snd_conidx].unique_ == rmsg.sender_con_id_.unique);

                    {
                        ConnectionStub& rsndcon = impl_->con_dq_[snd_conidx];
                        RelayData*      prd;
                        bool            should_notify_connection = rmsg.pfront_ == nullptr;

                        while ((prd = rmsg.pop())) {
                            prd->pnext_                = rsndcon.pdone_relay_data_top_;
                            rcon.pdone_relay_data_top_ = prd;
                        }

                        should_notify_connection = should_notify_connection and (rmsg.pfront_ != nullptr);

                        {
                            const bool sndcon_already_has_canceled_messages = rsndcon.send_msg_list_.front().state_ == MessageStateE::RecvCancel;
                            should_notify_connection                        = should_notify_connection or not sndcon_already_has_canceled_messages;
                        }

                        rsndcon.send_msg_list_.erase(msgidx);
                        rsndcon.send_msg_list_.pushFront(msgidx);
                        SOLID_ASSERT(rsndcon.send_msg_list_.check());

                        if (should_notify_connection) {
                            SOLID_CHECK(notifyConnection(_rsvc, rcon.id_, RelayEngineNotification::DoneData), "Connection should be alive");
                        }
                    }
                    continue;
                //case MessageStateE::SendCancel://rmsg.sender_con_id_ should be invalid
                //case MessageStateE::RecvCancel://message should not be in the recv_msg_list_
                default:
                    SOLID_CHECK(false, "Invalid message state " << static_cast<size_t>(rmsg.state_));
                    break;
                } //switch
            }
            //simply erase the message
            RelayData* prd;
            while ((prd = rmsg.pop())) {
                impl_->freeRelayData(prd);
            }
            rmsg.clear();
            impl_->freeMessage(msgidx);
        } //while
    }

    {
        while (rcon.send_msg_list_.size()) {
            MessageStub& rmsg       = rcon.send_msg_list_.front();
            const size_t msgidx     = rcon.send_msg_list_.popFront();
            const size_t rcv_conidx = rmsg.receiver_con_id_.index;
            SOLID_ASSERT(rcon.send_msg_list_.check());

            rmsg.sender_con_id_.clear(); //unlink from the sender connection

            //clean message relay data
            RelayData* prd;
            while ((prd = rmsg.pop())) {
                impl_->freeRelayData(prd);
            }

            if (rmsg.receiver_con_id_.isValid()) {
                switch (rmsg.state_) {
                case MessageStateE::Relay:
                case MessageStateE::WaitResponse:
                    rmsg.state_ = MessageStateE::SendCancel;

                    SOLID_ASSERT(rcv_conidx < impl_->con_dq_.size() and impl_->con_dq_[rcv_conidx].unique_ == rmsg.receiver_con_id_.unique);

                    {
                        ConnectionStub& rrcvcon                  = impl_->con_dq_[rcv_conidx];
                        bool            should_notify_connection = rrcvcon.send_msg_list_.empty();

                        rrcvcon.recv_msg_list_.erase(msgidx);
                        rrcvcon.recv_msg_list_.pushBack(msgidx);

                        if (should_notify_connection) {
                            SOLID_CHECK(notifyConnection(_rsvc, rcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
                        }
                    }
                    continue;
                //case MessageStateE::SendCancel://rmsg.sender_con_id_ should be invalid
                //case MessageStateE::RecvCancel://message should not be in the recv_msg_list_
                default:
                    SOLID_CHECK(false, "Invalid message state " << static_cast<size_t>(rmsg.state_));
                    break;
                } //switch
            }
            //simply erase the message
            rmsg.clear();
            impl_->freeMessage(msgidx);
        } //while
    }

    {
        //clean-up done relay data
        RelayData* prd = rcon.pdone_relay_data_top_;

        idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " conidx = " << conidx);

        while (prd) {
            RelayData* ptmprd = prd->pnext_;
            prd->clear();
            impl_->freeRelayData(prd);
            prd = ptmprd;
        }
    }

    if (not rcon.name_.empty()) {
        impl_->con_umap_.erase(rcon.name_.c_str());
    }

    impl_->con_id_umap_.erase(rcon.id_.index);

    rcon.clear();
    impl_->con_cache_.push(_rconuid.index);
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(Service& _rsvc, const ObjectIdT& _rconuid, std::string&& _uname)
{
    to_lower(_uname);
    unique_lock<mutex> lock(impl_->mtx_);

    size_t conidx = InvalidIndex();
    {
        const auto it = impl_->con_id_umap_.find(_rconuid.index);

        if (it != impl_->con_id_umap_.end()) {
            const size_t    tmp_conidx = it->second;
            ConnectionStub& rcon       = impl_->con_dq_[tmp_conidx];

            if (rcon.id_.unique == _rconuid.unique) {
                conidx = tmp_conidx;
            }
        }
    }
    if (not _uname.empty()) {
        const auto it = impl_->con_umap_.find(_uname.c_str());

        if (it != impl_->con_umap_.end()) {
            //TODO: see "CONFLICT scenario" below
            SOLID_ASSERT(conidx == InvalidIndex() or conidx == it->second);
            conidx = it->second;
        } else if (conidx != InvalidIndex()) {
            impl_->con_dq_[conidx].name_                           = std::move(_uname);
            impl_->con_umap_[impl_->con_dq_[conidx].name_.c_str()] = conidx;
        } else if (impl_->con_cache_.size()) {
            conidx = impl_->con_cache_.top();
            impl_->con_cache_.pop();
            impl_->con_dq_[conidx].name_                           = std::move(_uname);
            impl_->con_umap_[impl_->con_dq_[conidx].name_.c_str()] = conidx;
        } else {
            conidx = impl_->con_dq_.size();
            impl_->con_dq_.emplace_back(impl_->msg_dq_, std::move(_uname));
            impl_->con_umap_[impl_->con_dq_.back().name_.c_str()] = conidx;
        }
    } else {
        //TODO:
        conidx = doRegisterConnection(_rconuid);
        SOLID_ASSERT(false);
    }

    impl_->con_id_umap_[_rconuid.index] = conidx;

    ConnectionStub& rcon = impl_->con_dq_[conidx];
    rcon.id_             = _rconuid;

    if (not rcon.recv_msg_list_.empty()) {
        SOLID_CHECK(notifyConnection(_rsvc, rcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
    }

    idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " conidx = " << conidx);
}
//-----------------------------------------------------------------------------
size_t RelayEngine::doRegisterConnection(const ObjectIdT& _rconuid)
{
    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    if (it != impl_->con_id_umap_.end()) {
        const size_t    conidx = it->second;
        ConnectionStub& rcon   = impl_->con_dq_[conidx];

        if (rcon.id_.unique == _rconuid.unique) {
            vdbgx(Debug::mpipc, conidx);
            return conidx;
        }
    }

    size_t conidx = InvalidIndex();

    if (impl_->con_cache_.size()) {
        conidx = impl_->con_cache_.top();
        impl_->con_cache_.pop();
        impl_->con_dq_[conidx].name_.clear();
    } else {
        conidx = impl_->con_dq_.size();
        impl_->con_dq_.emplace_back(std::ref(impl_->msg_dq_));
    }

    impl_->con_dq_[conidx].id_          = _rconuid;
    impl_->con_id_umap_[_rconuid.index] = conidx;
    vdbgx(Debug::mpipc, conidx);
    return conidx;
}
//-----------------------------------------------------------------------------
size_t RelayEngine::doRegisterConnection(std::string&& _uname)
{
    size_t     conidx = InvalidIndex();
    const auto it     = impl_->con_umap_.find(_uname.c_str());

    if (it != impl_->con_umap_.end()) {
        conidx = it->second;
    } else if (impl_->con_cache_.size()) {
        conidx = impl_->con_cache_.top();
        impl_->con_cache_.pop();
        impl_->con_dq_[conidx].name_                           = std::move(_uname);
        impl_->con_umap_[impl_->con_dq_[conidx].name_.c_str()] = conidx;
    } else {
        conidx = impl_->con_dq_.size();
        impl_->con_dq_.emplace_back(impl_->msg_dq_, std::move(_uname));
        impl_->con_umap_[impl_->con_dq_.back().name_.c_str()] = conidx;
    }
    idbgx(Debug::mpipc, "name = " << impl_->con_dq_[conidx].name_ << " conidx = " << conidx);
    return conidx;
}
//-----------------------------------------------------------------------------
// called by sending connection on new relayed message
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

    to_lower(_rmsghdr.url_);

    size_t     snd_conidx = InvalidIndex();
    const auto it         = impl_->con_id_umap_.find(_rconuid.index);
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
        snd_conidx = doRegisterConnection(_rconuid);
    } else {
        snd_conidx = it->second;
    }
    msgidx            = impl_->createMessage();
    MessageStub& rmsg = impl_->msg_dq_[msgidx];

    rmsg.header_ = std::move(_rmsghdr);
    rmsg.state_  = MessageStateE::Relay;

    _rrelay_id = MessageId(msgidx, rmsg.unique_);

    const size_t    rcv_conidx = doRegisterConnection(std::move(rmsg.header_.url_));
    ConnectionStub& rrcvcon    = impl_->con_dq_[rcv_conidx];
    ConnectionStub& rsndcon    = impl_->con_dq_[snd_conidx];

    //also hold the in-engine connection id into msg
    rmsg.sender_con_id_   = ObjectIdT(snd_conidx, impl_->con_dq_[snd_conidx].unique_);
    rmsg.receiver_con_id_ = ObjectIdT(rcv_conidx, rrcvcon.unique_);

    //register message onto sender connection:
    rsndcon.send_msg_list_.pushBack(msgidx);

    idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " size = " << _rrelmsg.data_size_ << " receiver_conidx " << rmsg.receiver_con_id_.index << " sender_conidx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_);

    SOLID_ASSERT(rmsg.pfront_ == nullptr);

    rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

    bool should_notify_connection = (rrcvcon.recv_msg_list_.empty() or not rrcvcon.recv_msg_list_.back().isActive());

    rrcvcon.recv_msg_list_.pushBack(msgidx);

    if (should_notify_connection) {
        if (rrcvcon.id_.isValid()) {
            SOLID_CHECK(notifyConnection(_rsvc, rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
        }
    }

    return true;
}
//-----------------------------------------------------------------------------
// called by sending connection on new relay data for an existing message
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
        size_t       data_size                     = _rrelmsg.data_size_;

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        if (rmsg.state_ == MessageStateE::Relay) {

            idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " rcv_conidx " << rmsg.receiver_con_id_.index << " snd_conidx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_ << " is_mrq_empty = " << is_msg_relay_data_queue_empty << " dsz = " << data_size);

            if (is_msg_relay_data_queue_empty) {
                ConnectionStub& rrcvcon                  = impl_->con_dq_[rmsg.receiver_con_id_.index];
                bool            should_notify_connection = (rrcvcon.recv_msg_list_.backIndex() == msgidx or not rrcvcon.recv_msg_list_.back().isActive());

                SOLID_ASSERT(not rrcvcon.recv_msg_list_.empty());

                //move the message at the back of the list so it get processed sooner - somewhat unfair
                //but this way we can keep using a single list for send messages
                rrcvcon.recv_msg_list_.erase(msgidx);
                rrcvcon.recv_msg_list_.pushBack(msgidx);
                idbgx(Debug::mpipc, "rcv_lst = " << rrcvcon.recv_msg_list_ << " notify_conn = " << should_notify_connection);

                if (should_notify_connection) {
                    //idbgx(Debug::mpipc, _rconuid <<" notify receiver connection");
                    SOLID_CHECK(notifyConnection(_rsvc, rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
                }
            }
        } else {
            //we do not return false because the connection is about to be notified
            //about message state change
            wdbgx(Debug::mpipc, _rconuid << " message not in relay state instead in " << static_cast<size_t>(rmsg.state_));
        }
        return true;
    } else {
        edbgx(Debug::mpipc, _rconuid << " message not found " << _rrelay_id);
        return false;
    }
}
//-----------------------------------------------------------------------------
// called by receiving connection on relaying the message response
// after this call, the receiving and sending connections, previously
// associtate to the message are swapped (the receiving connection becomes the sending and
// the sending one becomes receving)
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
        idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_conidx " << rmsg.receiver_con_id_ << " sender_conidx " << rmsg.sender_con_id_ << " is_last = " << _rrelmsg.is_last_);

        if (rmsg.state_ == MessageStateE::WaitResponse) {
            SOLID_ASSERT(rmsg.sender_con_id_.isValid());
            SOLID_ASSERT(rmsg.receiver_con_id_.isValid());
            SOLID_ASSERT(rmsg.pfront_ == nullptr);

            rmsg.receiver_msg_id_.clear();

            std::swap(rmsg.receiver_con_id_, rmsg.sender_con_id_);
            rmsg.header_ = std::move(_rmsghdr);
            //set the proper recipient_request_id_
            rmsg.header_.sender_request_id_ = sender_request_id;

            rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));
            rmsg.state_ = MessageStateE::Relay;

            const size_t rcv_conidx = rmsg.receiver_con_id_.index;
            const size_t snd_conidx = rmsg.sender_con_id_.index;
            SOLID_ASSERT(rcv_conidx < impl_->con_dq_.size() and impl_->con_dq_[rcv_conidx].unique_ == rmsg.receiver_con_id_.unique);
            SOLID_ASSERT(snd_conidx < impl_->con_dq_.size() and impl_->con_dq_[snd_conidx].unique_ == rmsg.sender_con_id_.unique);

            ConnectionStub& rrcvcon                  = impl_->con_dq_[rcv_conidx];
            ConnectionStub& rsndcon                  = impl_->con_dq_[snd_conidx];
            bool            should_notify_connection = rrcvcon.recv_msg_list_.empty() or not rrcvcon.recv_msg_list_.back().isActive();

            rsndcon.recv_msg_list_.erase(msgidx); //
            rrcvcon.send_msg_list_.erase(msgidx); //MUST do the erase before push!!!
            rsndcon.send_msg_list_.pushBack(msgidx); //
            rrcvcon.recv_msg_list_.pushBack(msgidx); //

            idbgx(Debug::mpipc, "rcv_lst = " << rrcvcon.recv_msg_list_);

            SOLID_ASSERT(rsndcon.send_msg_list_.check());
            SOLID_ASSERT(rrcvcon.send_msg_list_.check());

            if (should_notify_connection) {
                SOLID_CHECK(notifyConnection(_rsvc, rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
            }
        }
        return true;
    } else {
        edbgx(Debug::mpipc, "message not found");
        return false;
    }
}
//-----------------------------------------------------------------------------
// called by the receiver connection on new relay data
void RelayEngine::doPollNew(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);
    //TODO: optimize the search
    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    SOLID_ASSERT(it != impl_->con_id_umap_.end()); //the current connection must be registered

    const size_t    conidx = it->second;
    ConnectionStub& rcon   = impl_->con_dq_[conidx];

    SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);

    idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

    bool   can_retry = true;
    size_t msgidx    = rcon.recv_msg_list_.backIndex();

    while (can_retry and msgidx != InvalidIndex() and impl_->msg_dq_[msgidx].isActive()) {
        MessageStub& rmsg        = impl_->msg_dq_[msgidx];
        const size_t prev_msgidx = rcon.recv_msg_list_.previousIndex(msgidx);
        RelayData*   pnext       = rmsg.pfront_ ? rmsg.pfront_->pnext_ : nullptr;

        if (_try_push_fnc(rmsg.pfront_, MessageId(msgidx, rmsg.unique_), rmsg.receiver_msg_id_, can_retry)) {

            rmsg.pfront_ = pnext;

            if (rmsg.pfront_ == nullptr) {
                rmsg.pback_ = nullptr;
            }

            //relay data accepted
            if (rmsg.state_ == MessageStateE::SendCancel) {
                //delete the message
                rcon.recv_msg_list_.erase(msgidx);
                rmsg.clear();
                impl_->freeMessage(msgidx);
            } else if (rmsg.pfront_) {
                //message has more data - leave it where it is on the list
            } else {
                //message has no more data, move it to front
                rcon.recv_msg_list_.erase(msgidx);
                rcon.recv_msg_list_.pushFront(msgidx);
            }
        }
        msgidx = prev_msgidx;
    } //while

    _rmore = rcon.recv_msg_list_.size() and rcon.recv_msg_list_.back().isActive();
    idbgx(Debug::mpipc, _rconuid << " more = " << _rmore << " rcv_lst = " << rcon.recv_msg_list_);
}
//-----------------------------------------------------------------------------
// called by sender connection when either
// have completed relaydata (the receiving connections have sent the data) or
// have messages canceled by receiving connections
void RelayEngine::doPollDone(const ObjectIdT& _rconuid, DoneFunctionT& _done_fnc, CancelFunctionT& _cancel_fnc)
{
    unique_lock<mutex> lock(impl_->mtx_);

    const auto it = impl_->con_id_umap_.find(_rconuid.index);

    SOLID_ASSERT(it != impl_->con_id_umap_.end()); //the current connection must be registered
    const size_t    conidx = it->second;
    ConnectionStub& rcon   = impl_->con_dq_[conidx];

    idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);
    SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);

    RelayData* prd = rcon.pdone_relay_data_top_;

    while (prd) {
        _done_fnc(prd->bufptr_);
        RelayData* ptmprd = prd->pnext_;
        prd->clear();
        impl_->freeRelayData(prd);
        prd = ptmprd;
    }
    rcon.pdone_relay_data_top_ = nullptr;
    idbgx(Debug::mpipc, _rconuid << " send_msg_list.size = " << rcon.send_msg_list_.size() << " front idx = " << rcon.send_msg_list_.frontIndex());

    SOLID_ASSERT(rcon.send_msg_list_.check());

    while (rcon.send_msg_list_.size() and rcon.send_msg_list_.front().state_ == MessageStateE::RecvCancel) {
        MessageStub& rmsg   = rcon.send_msg_list_.front();
        size_t       msgidx = rcon.send_msg_list_.popFront();

        SOLID_ASSERT(rmsg.receiver_con_id_.isInvalid());

        _cancel_fnc(rmsg.header_);

        rmsg.clear();
        impl_->freeMessage(msgidx);
    }
}
//-----------------------------------------------------------------------------
// called by receiving connection after using the relay_data
void RelayEngine::doComplete(
    Service&         _rsvc,
    const ObjectIdT& _rconuid,
    RelayData*       _prelay_data,
    MessageId const& _rengine_msg_id,
    bool&            _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);

    idbgx(Debug::mpipc, _rconuid << " try complete msg " << _rengine_msg_id);
    SOLID_ASSERT(_prelay_data);

    if (_rengine_msg_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rengine_msg_id.index].unique_ == _rengine_msg_id.unique) {
        const size_t msgidx = _rengine_msg_id.index;
        MessageStub& rmsg   = impl_->msg_dq_[msgidx];

        if (rmsg.sender_con_id_.isValid()) {
            SOLID_ASSERT(rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique);

            ConnectionStub& rsndcon                  = impl_->con_dq_[rmsg.sender_con_id_.index];
            ConnectionStub& rrcvcon                  = impl_->con_dq_[rmsg.receiver_con_id_.index]; //the connection currently calling this method
            bool            should_notify_connection = rsndcon.pdone_relay_data_top_ == nullptr;

            _prelay_data->pnext_          = rsndcon.pdone_relay_data_top_;
            rsndcon.pdone_relay_data_top_ = _prelay_data;

            if (should_notify_connection) {
                SOLID_CHECK(notifyConnection(_rsvc, rsndcon.id_, RelayEngineNotification::DoneData), "Connection should be alive");
            }

            if (_prelay_data->is_last_) {
                SOLID_ASSERT(rmsg.pfront_ == nullptr);

                if (Message::is_waiting_response(_prelay_data->pmessage_header_->flags_)) {
                    rmsg.state_ = MessageStateE::WaitResponse;

                    rrcvcon.recv_msg_list_.erase(msgidx);
                    rrcvcon.recv_msg_list_.pushFront(msgidx);
                    idbgx(Debug::mpipc, _rconuid << " waitresponse " << msgidx << " rcv_lst = " << rsndcon.send_msg_list_);
                } else {
                    rrcvcon.recv_msg_list_.erase(msgidx);
                    rsndcon.send_msg_list_.erase(msgidx);
                    SOLID_ASSERT(rsndcon.send_msg_list_.check());
                    rmsg.clear();
                    impl_->freeMessage(msgidx);
                    idbgx(Debug::mpipc, _rconuid << " erase message " << msgidx << " rcv_lst = " << rsndcon.send_msg_list_);
                }
            }

            _rmore = rrcvcon.recv_msg_list_.size() and rrcvcon.recv_msg_list_.back().isActive();
            idbgx(Debug::mpipc, _rconuid << " " << _rengine_msg_id << " more " << _rmore << " rcv_lst = " << rrcvcon.recv_msg_list_);
            return;
        }
    }

    _prelay_data->clear();
    impl_->freeRelayData(_prelay_data);
    edbgx(Debug::mpipc, _rconuid << " message not found " << _rengine_msg_id);
}
//-----------------------------------------------------------------------------
// called by the receiving connection (the one forwarding the relay data) on close
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
        rmsg.push(_prelay_data);
        //do nothing for now - leave the processing to connectionStop method
    } else {
        _prelay_data->clear();
        impl_->freeRelayData(_prelay_data);
        edbgx(Debug::mpipc, _rconuid << " message not found " << _rengine_msg_id);
    }
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
