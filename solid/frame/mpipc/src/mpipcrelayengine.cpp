// solid/frame/ipc/src/mpipcrelayengine.cpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
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
namespace relay {
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
        * the connection must call EngineCore::doCompleteClose on all relayed messages in its writer
        *   then EngineCore::connectionStop is called.
        - Ideas:
            - on doCompleteClose can add _prelay_data back to message list and add the message to the receiver connection message list
            - on connectionStop
    * sender connection receives a message canceled command
        * 
    * receiver connection receives message cancel request command which should be forwarder up to the initial sender of the message
    
    W1---->RR-->RW---->R2
    R1<----RW<--RR<----W2
    
    W1 - Writer for connection 1
    R1 - Reader for connection 1
    RR - Relay reader
    RW - Relay writer
    R2 - Reader for connection 2
    W2 - Writer for connection 2
    
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
    UniqueId      sender_con_id_;
    UniqueId      receiver_con_id_;
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
        return state_ == MessageStateE::RecvCancel || state_ == MessageStateE::SendCancel;
    }

    bool hasData() const
    {
        return pback_ != nullptr || state_ == MessageStateE::SendCancel;
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

struct ConnectionStub : ConnectionStubBase {
    uint32_t       unique_;
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
        : ConnectionStubBase(std::move(_uname))
        , unique_(0)
        , pdone_relay_data_top_(nullptr)
        , send_msg_list_(_rmsg_dq)
        , recv_msg_list_(_rmsg_dq)
    {
    }

    void clear()
    {
        ++unique_;

        id_.clear();
        ConnectionStubBase::clear();
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
} //namespace

std::ostream& operator<<(std::ostream& _ros, const ConnectionPrintStub& _rps)
{
    return _rps.re_.print(_ros, _rps.rc_);
}

struct EngineCore::Data {
    Manager&         rm_;
    mutex            mtx_;
    MessageDequeT    msg_dq_;
    RelayDataDequeT  reldata_dq_;
    RelayData*       prelay_data_cache_top_;
    SendInnerListT   msg_cache_inner_list_;
    ConnectionDequeT con_dq_;
    ConnectionMapT   con_umap_;
    SizeTStackT      con_cache_;

    Data(Manager& _rm)
        : rm_(_rm)
        , prelay_data_cache_top_(nullptr)
        , msg_cache_inner_list_(msg_dq_)
    {
    }

    Manager& manager() const
    {
        return rm_;
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
    RelayData* createSendCancelRelayData()
    {
        RelayData* prd = nullptr;
        if (prelay_data_cache_top_) {
            prd                    = prelay_data_cache_top_;
            prelay_data_cache_top_ = prelay_data_cache_top_->pnext_;
            prd->clear();
        } else {
            reldata_dq_.emplace_back();
            prd = &reldata_dq_.back();
        }

        prd->is_last_ = true;
        return prd;
    }
    void eraseRelayData(RelayData*& _prd)
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
    void eraseMessage(const size_t _idx)
    {
        msg_cache_inner_list_.pushBack(_idx);
    }

    size_t createConnection()
    {
        size_t conidx;
        if (con_cache_.size()) {
            conidx = con_cache_.top();
            con_cache_.pop();
        } else {
            conidx = con_dq_.size();
            con_dq_.emplace_back(msg_dq_);
        }
        return conidx;
    }

    void eraseConnection(const size_t _conidx)
    {
        con_dq_[_conidx].clear();
        con_cache_.push(_conidx);
    }
    bool isValid(const UniqueId& _rrelay_con_uid) const
    {
        return _rrelay_con_uid.index < con_dq_.size() and con_dq_[_rrelay_con_uid.index].unique_ == _rrelay_con_uid.unique;
    }
};
//-----------------------------------------------------------------------------
EngineCore::EngineCore(Manager& _rm)
    : impl_(make_pimpl<Data>(_rm))
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
EngineCore::~EngineCore()
{
    idbgx(Debug::mpipc, this);
}
//-----------------------------------------------------------------------------
Manager& EngineCore::manager()
{
    return impl_->manager();
}

//-----------------------------------------------------------------------------
void EngineCore::stopConnection(const UniqueId& _rrelay_con_uid)
{
    unique_lock<mutex> lock(impl_->mtx_);
    if (_rrelay_con_uid.isValid()) {
        doStopConnection(_rrelay_con_uid.index);
    }
}
//-----------------------------------------------------------------------------
void EngineCore::doStopConnection(const size_t _conidx)
{
    ConnectionStub& rcon = impl_->con_dq_[_conidx];
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
                        ConnectionStub& rsndcon                  = impl_->con_dq_[snd_conidx];
                        bool            should_notify_connection = msgidx == rsndcon.send_msg_list_.frontIndex() || rsndcon.send_msg_list_.front().state_ != MessageStateE::RecvCancel;

                        rsndcon.send_msg_list_.erase(msgidx);
                        rsndcon.send_msg_list_.pushFront(msgidx);
                        SOLID_ASSERT(rsndcon.send_msg_list_.check());

                        if (should_notify_connection) {
                            SOLID_CHECK(notifyConnection(impl_->manager(), rsndcon.id_, RelayEngineNotification::DoneData), "Connection should be alive");
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
                impl_->eraseRelayData(prd);
            }
            rmsg.clear();
            impl_->eraseMessage(msgidx);
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
                impl_->eraseRelayData(prd);
            }

            if (rmsg.receiver_con_id_.isValid()) {
                switch (rmsg.state_) {
                case MessageStateE::Relay:
                case MessageStateE::WaitResponse:
                    rmsg.state_ = MessageStateE::SendCancel;

                    SOLID_ASSERT(rcv_conidx < impl_->con_dq_.size() and impl_->con_dq_[rcv_conidx].unique_ == rmsg.receiver_con_id_.unique);
                    rmsg.push(impl_->createSendCancelRelayData());
                    {
                        ConnectionStub& rrcvcon                  = impl_->con_dq_[rcv_conidx];
                        bool            should_notify_connection = (rrcvcon.recv_msg_list_.backIndex() == msgidx || not rrcvcon.recv_msg_list_.back().hasData());

                        rrcvcon.recv_msg_list_.erase(msgidx);
                        rrcvcon.recv_msg_list_.pushBack(msgidx);

                        if (should_notify_connection) {
                            SOLID_CHECK(notifyConnection(impl_->manager(), rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
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
            impl_->eraseMessage(msgidx);
        } //while
    }

    {
        //clean-up done relay data
        RelayData* prd = rcon.pdone_relay_data_top_;

        idbgx(Debug::mpipc, _conidx << ' ' << plot(rcon));

        while (prd) {
            RelayData* ptmprd = prd->pnext_;
            prd->clear();
            impl_->eraseRelayData(prd);
            prd = ptmprd;
        }
    }
    {
        Proxy proxy(*this);
        unregisterConnectionName(proxy, _conidx);
    }

    impl_->eraseConnection(_conidx);
}
//-----------------------------------------------------------------------------
void EngineCore::doExecute(ExecuteFunctionT& _rfnc)
{
    Proxy              proxy(*this);
    unique_lock<mutex> lock(impl_->mtx_);
    _rfnc(proxy);
}
//-----------------------------------------------------------------------------
size_t EngineCore::doRegisterUnnamedConnection(const ObjectIdT& _rcon_uid, UniqueId& _rrelay_con_uid)
{
    if (_rrelay_con_uid.isValid()) {
        SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));
        return _rrelay_con_uid.index;
    }

    size_t          conidx = impl_->createConnection();
    ConnectionStub& rcon   = impl_->con_dq_[conidx];
    rcon.id_               = _rcon_uid;
    _rrelay_con_uid.index  = conidx;
    _rrelay_con_uid.unique = rcon.unique_;

    vdbgx(Debug::mpipc, _rrelay_con_uid << ' ' << plot(rcon));
    return conidx;
}
//-----------------------------------------------------------------------------
size_t EngineCore::doRegisterNamedConnection(std::string&& _uname)
{
    Proxy  proxy(*this);
    size_t conidx = registerConnection(proxy, std::move(_uname));
    idbgx(Debug::mpipc, conidx << ' ' << plot(impl_->con_dq_[conidx]));
    return conidx;
}
//-----------------------------------------------------------------------------
// called by sending connection on new relayed message
bool EngineCore::doRelayStart(
    const ObjectIdT& _rcon_uid,
    UniqueId&        _rrelay_con_uid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    MessageId&       _rrelay_id,
    ErrorConditionT& _rerror)
{
    size_t             msgidx;
    unique_lock<mutex> lock(impl_->mtx_);
    SOLID_ASSERT(_rcon_uid.isValid());

    size_t snd_conidx = _rrelay_con_uid.index;

    if (_rrelay_con_uid.isValid()) {
        SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));
        snd_conidx = _rrelay_con_uid.index;
    } else {
        snd_conidx = doRegisterUnnamedConnection(_rcon_uid, _rrelay_con_uid);
    }

    msgidx            = impl_->createMessage();
    MessageStub& rmsg = impl_->msg_dq_[msgidx];

    rmsg.header_ = std::move(_rmsghdr);
    rmsg.state_  = MessageStateE::Relay;

    //the request_ids are already swapped on deserialization
    std::swap(rmsg.header_.recipient_request_id_, rmsg.header_.sender_request_id_);

    _rrelay_id = MessageId(msgidx, rmsg.unique_);

    const size_t    rcv_conidx = doRegisterNamedConnection(std::move(rmsg.header_.url_));
    ConnectionStub& rrcvcon    = impl_->con_dq_[rcv_conidx];
    ConnectionStub& rsndcon    = impl_->con_dq_[snd_conidx];

    //also hold the in-engine connection id into msg
    rmsg.sender_con_id_   = ObjectIdT(snd_conidx, impl_->con_dq_[snd_conidx].unique_);
    rmsg.receiver_con_id_ = ObjectIdT(rcv_conidx, rrcvcon.unique_);

    //register message onto sender connection:
    rsndcon.send_msg_list_.pushBack(msgidx);

    idbgx(Debug::mpipc, _rrelay_con_uid << " msgid = " << _rrelay_id << " size = " << _rrelmsg.data_size_ << " receiver_conidx " << rmsg.receiver_con_id_.index << " sender_conidx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_);

    SOLID_ASSERT(rmsg.pfront_ == nullptr);

    rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

    bool should_notify_connection = (rrcvcon.recv_msg_list_.empty() || not rrcvcon.recv_msg_list_.back().hasData());

    rrcvcon.recv_msg_list_.pushBack(msgidx);

    if (should_notify_connection and rrcvcon.id_.isValid()) {
        SOLID_CHECK(notifyConnection(impl_->manager(), rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
    }

    return true;
}
//-----------------------------------------------------------------------------
// called by sending connection on new relay data for an existing message
bool EngineCore::doRelay(
    const UniqueId&  _rrelay_con_uid,
    RelayData&&      _rrelmsg,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_id.isValid());
    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
        const size_t msgidx                        = _rrelay_id.index;
        MessageStub& rmsg                          = impl_->msg_dq_[msgidx];
        bool         is_msg_relay_data_queue_empty = (rmsg.pfront_ == nullptr);
        size_t       data_size                     = _rrelmsg.data_size_;

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        if (rmsg.state_ == MessageStateE::Relay) {

            idbgx(Debug::mpipc, _rrelay_con_uid << " msgid = " << _rrelay_id << " rcv_conidx " << rmsg.receiver_con_id_.index << " snd_conidx " << rmsg.sender_con_id_.index << " is_last = " << _rrelmsg.is_last_ << " is_mrq_empty = " << is_msg_relay_data_queue_empty << " dsz = " << data_size);

            if (is_msg_relay_data_queue_empty) {
                ConnectionStub& rrcvcon                  = impl_->con_dq_[rmsg.receiver_con_id_.index];
                bool            should_notify_connection = (rrcvcon.recv_msg_list_.backIndex() == msgidx || not rrcvcon.recv_msg_list_.back().hasData());

                SOLID_ASSERT(not rrcvcon.recv_msg_list_.empty());

                //move the message at the back of the list so it get processed sooner - somewhat unfair
                //but this way we can keep using a single list for send messages
                rrcvcon.recv_msg_list_.erase(msgidx);
                rrcvcon.recv_msg_list_.pushBack(msgidx);
                idbgx(Debug::mpipc, "rcv_lst = " << rrcvcon.recv_msg_list_ << " notify_conn = " << should_notify_connection);

                if (should_notify_connection) {
                    //idbgx(Debug::mpipc, _rrelay_con_uid <<" notify receiver connection");
                    SOLID_CHECK(notifyConnection(impl_->manager(), rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
                }
            }
        } else {
            //we do not return false because the connection is about to be notified
            //about message state change
            wdbgx(Debug::mpipc, _rrelay_con_uid << " message not in relay state instead in " << static_cast<size_t>(rmsg.state_));
        }
        return true;
    } else {
        edbgx(Debug::mpipc, _rrelay_con_uid << " message not found " << _rrelay_id);
        return false;
    }
}
//-----------------------------------------------------------------------------
// called by receiving connection on relaying the message response
// after this call, the receiving and sending connections, previously
// associtate to the message are swapped (the receiving connection becomes the sending and
// the sending one becomes receving)
bool EngineCore::doRelayResponse(
    const UniqueId&  _rrelay_con_uid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_id.isValid());
    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
        const size_t msgidx            = _rrelay_id.index;
        MessageStub& rmsg              = impl_->msg_dq_[msgidx];
        RequestId    sender_request_id = rmsg.header_.recipient_request_id_; //the request ids were swapped on doRelayStart
        idbgx(Debug::mpipc, _rrelay_con_uid << " msgid = " << _rrelay_id << " receiver_conidx " << rmsg.receiver_con_id_ << " sender_conidx " << rmsg.sender_con_id_ << " is_last = " << _rrelmsg.is_last_);

        if (rmsg.state_ == MessageStateE::WaitResponse) {
            SOLID_ASSERT(rmsg.sender_con_id_.isValid());
            SOLID_ASSERT(rmsg.receiver_con_id_.isValid());
            SOLID_ASSERT(rmsg.pfront_ == nullptr);

            rmsg.receiver_msg_id_.clear();

            std::swap(rmsg.receiver_con_id_, rmsg.sender_con_id_);

            rmsg.header_ = std::move(_rmsghdr);

            std::swap(rmsg.header_.recipient_request_id_, rmsg.header_.sender_request_id_);

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
            bool            should_notify_connection = rrcvcon.recv_msg_list_.empty() || not rrcvcon.recv_msg_list_.back().hasData();

            rsndcon.recv_msg_list_.erase(msgidx); //
            rrcvcon.send_msg_list_.erase(msgidx); //MUST do the erase before push!!!
            rsndcon.send_msg_list_.pushBack(msgidx); //
            rrcvcon.recv_msg_list_.pushBack(msgidx); //

            idbgx(Debug::mpipc, "rcv_lst = " << rrcvcon.recv_msg_list_);

            SOLID_ASSERT(rsndcon.send_msg_list_.check());
            SOLID_ASSERT(rrcvcon.send_msg_list_.check());

            if (should_notify_connection) {
                SOLID_CHECK(notifyConnection(impl_->manager(), rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
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
void EngineCore::doPollNew(const UniqueId& _rrelay_con_uid, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    const size_t    conidx = _rrelay_con_uid.index;
    ConnectionStub& rcon   = impl_->con_dq_[conidx];

    SOLID_ASSERT(rcon.id_.unique == _rrelay_con_uid.unique);

    idbgx(Debug::mpipc, _rrelay_con_uid << ' ' << plot(rcon));

    bool   can_retry = true;
    size_t msgidx    = rcon.recv_msg_list_.backIndex();

    while (can_retry and msgidx != InvalidIndex() and impl_->msg_dq_[msgidx].hasData()) {
        MessageStub& rmsg        = impl_->msg_dq_[msgidx];
        const size_t prev_msgidx = rcon.recv_msg_list_.previousIndex(msgidx);
        RelayData*   pnext       = rmsg.pfront_ ? rmsg.pfront_->pnext_ : nullptr;

        if (_try_push_fnc(rmsg.pfront_, MessageId(msgidx, rmsg.unique_), rmsg.receiver_msg_id_, can_retry)) {
            if (rmsg.pfront_ == nullptr) {
                rmsg.pfront_ = pnext;

                if (rmsg.pfront_ == nullptr) {
                    rmsg.pback_ = nullptr;
                }

                //relay data accepted
                if (rmsg.pfront_) {
                    //message has more data - leave it where it is on the list
                } else {
                    //message has no more data, move it to front
                    rcon.recv_msg_list_.erase(msgidx);
                    rcon.recv_msg_list_.pushFront(msgidx);
                }
            } else {
                //the connection has received the SendCancel event for the message,
                //we can now safely delete the message
                SOLID_ASSERT(not rmsg.pfront_->bufptr_);
                SOLID_ASSERT(pnext == nullptr);

                if (rmsg.sender_con_id_.isValid()) {
                    //we can safely unlink message from sender connection
                    SOLID_ASSERT(rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique);

                    ConnectionStub& rsndcon = impl_->con_dq_[rmsg.sender_con_id_.index];

                    rsndcon.send_msg_list_.erase(msgidx);
                }

                rmsg.pfront_->clear();
                impl_->eraseRelayData(rmsg.pfront_);
                rmsg.pback_ = nullptr;
                rcon.recv_msg_list_.erase(msgidx);
                rmsg.clear();
                impl_->eraseMessage(msgidx);
                edbgx(Debug::mpipc, _rrelay_con_uid << " erase msg " << msgidx << " rcv_lst = " << rcon.recv_msg_list_);
            }
        }
        msgidx = prev_msgidx;
    } //while

    _rmore = rcon.recv_msg_list_.size() and rcon.recv_msg_list_.back().hasData();
    idbgx(Debug::mpipc, _rrelay_con_uid << " more = " << _rmore << " rcv_lst = " << rcon.recv_msg_list_);
}
//-----------------------------------------------------------------------------
// called by sender connection when either
// have completed relaydata (the receiving connections have sent the data) or
// have messages canceled by receiving connections
void EngineCore::doPollDone(const UniqueId& _rrelay_con_uid, DoneFunctionT& _done_fnc, CancelFunctionT& _cancel_fnc)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    const size_t    conidx = _rrelay_con_uid.index;
    ConnectionStub& rcon   = impl_->con_dq_[conidx];

    idbgx(Debug::mpipc, _rrelay_con_uid << ' ' << plot(rcon));

    RelayData* prd = rcon.pdone_relay_data_top_;

    while (prd) {
        _done_fnc(prd->bufptr_);
        RelayData* ptmprd = prd->pnext_;
        prd->clear();
        impl_->eraseRelayData(prd);
        prd = ptmprd;
    }
    rcon.pdone_relay_data_top_ = nullptr;
    idbgx(Debug::mpipc, _rrelay_con_uid << " send_msg_list.size = " << rcon.send_msg_list_.size() << " front idx = " << rcon.send_msg_list_.frontIndex());

    SOLID_ASSERT(rcon.send_msg_list_.check());

    while (rcon.send_msg_list_.size() and rcon.send_msg_list_.front().state_ == MessageStateE::RecvCancel) {
        MessageStub& rmsg   = rcon.send_msg_list_.front();
        size_t       msgidx = rcon.send_msg_list_.popFront();

        SOLID_ASSERT(rmsg.receiver_con_id_.isInvalid());

        while ((prd = rmsg.pop())) {
            _done_fnc(prd->bufptr_);
            prd->clear();
            impl_->eraseRelayData(prd);
        }
        _cancel_fnc(rmsg.header_);

        rmsg.clear();
        impl_->eraseMessage(msgidx);
    }
}
//-----------------------------------------------------------------------------
// called by receiving connection after using the relay_data
void EngineCore::doComplete(
    const UniqueId&  _rrelay_con_uid,
    RelayData*       _prelay_data,
    MessageId const& _rengine_msg_id,
    bool&            _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    idbgx(Debug::mpipc, _rrelay_con_uid << " try complete msg " << _rengine_msg_id);
    SOLID_ASSERT(_prelay_data);

    if (_rengine_msg_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rengine_msg_id.index].unique_ == _rengine_msg_id.unique) {
        const size_t    msgidx  = _rengine_msg_id.index;
        MessageStub&    rmsg    = impl_->msg_dq_[msgidx];
        ConnectionStub& rrcvcon = impl_->con_dq_[rmsg.receiver_con_id_.index]; //the connection currently calling this method

        if (rmsg.sender_con_id_.isValid()) {
            SOLID_ASSERT(rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique);

            ConnectionStub& rsndcon                  = impl_->con_dq_[rmsg.sender_con_id_.index];
            bool            should_notify_connection = rsndcon.pdone_relay_data_top_ == nullptr;

            _prelay_data->pnext_          = rsndcon.pdone_relay_data_top_;
            rsndcon.pdone_relay_data_top_ = _prelay_data;

            if (should_notify_connection) {
                SOLID_CHECK(notifyConnection(impl_->manager(), rsndcon.id_, RelayEngineNotification::DoneData), "Connection should be alive");
            }

            if (_prelay_data->is_last_) {
                SOLID_ASSERT(rmsg.pfront_ == nullptr);

                if (rmsg.state_ == MessageStateE::Relay and Message::is_waiting_response(_prelay_data->pmessage_header_->flags_)) {
                    rmsg.state_ = MessageStateE::WaitResponse;

                    rrcvcon.recv_msg_list_.erase(msgidx);
                    rrcvcon.recv_msg_list_.pushFront(msgidx);
                    idbgx(Debug::mpipc, _rrelay_con_uid << " waitresponse " << msgidx << " rcv_lst = " << rsndcon.send_msg_list_);
                } else {
                    rrcvcon.recv_msg_list_.erase(msgidx);
                    rsndcon.send_msg_list_.erase(msgidx);
                    SOLID_ASSERT(rsndcon.send_msg_list_.check());
                    rmsg.clear();
                    impl_->eraseMessage(msgidx);
                    idbgx(Debug::mpipc, _rrelay_con_uid << " erase message " << msgidx << " rcv_lst = " << rsndcon.send_msg_list_);
                }
            }

            _rmore = rrcvcon.recv_msg_list_.size() and rrcvcon.recv_msg_list_.back().hasData();
            idbgx(Debug::mpipc, _rrelay_con_uid << " " << _rengine_msg_id << " more " << _rmore << " rcv_lst = " << rrcvcon.recv_msg_list_);
            return;
        }
        _rmore = rrcvcon.recv_msg_list_.size() and rrcvcon.recv_msg_list_.back().hasData();
        idbgx(Debug::mpipc, _rrelay_con_uid << " " << _rengine_msg_id << " more " << _rmore << " rcv_lst = " << rrcvcon.recv_msg_list_);
    }
    //it happens for canceled relayed messages - see MessageWriter::doWriteRelayedCancelRequest
    _prelay_data->clear();
    impl_->eraseRelayData(_prelay_data);
    idbgx(Debug::mpipc, _rrelay_con_uid << " message not found " << _rengine_msg_id);
}
//-----------------------------------------------------------------------------
// called by any of connection when either:
// sending peer stops/cancels sending the message - MessageReader::Receiver::cancelRelayed
// receiver side, request canceling the message - MessageWriter::Sender::cancelRelayed
//TODO: add _rmore support as for doComplete
void EngineCore::doCancel(
    const UniqueId&  _rrelay_con_uid,
    RelayData*       _prelay_data,
    MessageId const& _rengine_msg_id,
    DoneFunctionT&   _done_fnc)
{
    unique_lock<mutex> lock(impl_->mtx_);

    SOLID_ASSERT(_rrelay_con_uid.isValid());
    SOLID_ASSERT(impl_->isValid(_rrelay_con_uid));

    idbgx(Debug::mpipc, _rrelay_con_uid << " try complete cancel msg " << _rengine_msg_id);
    const size_t msgidx = _rengine_msg_id.index;

    if (msgidx < impl_->msg_dq_.size() and impl_->msg_dq_[msgidx].unique_ == _rengine_msg_id.unique) {
        MessageStub& rmsg = impl_->msg_dq_[msgidx];
        //need to findout on which side we are - sender or receiver
        if (rmsg.sender_con_id_ == _rrelay_con_uid) {
            ConnectionStub& rsndcon = impl_->con_dq_[rmsg.sender_con_id_.index];

            //cancels comes from the sender connection
            //Problem
            // we cannot unlink the connection from the sender connection, because the receiving connection
            // might have already captured a message buffer which MUST return to the sender connection

            RelayData* prd;
            while ((prd = rmsg.pop())) {
                prd->pnext_                   = rsndcon.pdone_relay_data_top_;
                rsndcon.pdone_relay_data_top_ = prd;
            }
            prd = rsndcon.pdone_relay_data_top_;

            while (prd) {
                _done_fnc(prd->bufptr_);
                RelayData* ptmprd = prd->pnext_;
                prd->clear();
                impl_->eraseRelayData(prd);
                prd = ptmprd;
            }

            rsndcon.pdone_relay_data_top_ = nullptr;

            if (rmsg.receiver_con_id_.isValid()) {
                SOLID_ASSERT(rmsg.receiver_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.receiver_con_id_.index].unique_ == rmsg.receiver_con_id_.unique);

                ConnectionStub& rrcvcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
                rmsg.state_             = MessageStateE::SendCancel;

                rmsg.push(impl_->createSendCancelRelayData());
                {
                    bool should_notify_connection = (rrcvcon.recv_msg_list_.backIndex() == msgidx || not rrcvcon.recv_msg_list_.back().hasData());

                    rrcvcon.recv_msg_list_.erase(msgidx);
                    rrcvcon.recv_msg_list_.pushBack(msgidx);

                    if (should_notify_connection) {
                        idbgx(Debug::mpipc, _rrelay_con_uid << " notify recv connection of canceled message " << _rengine_msg_id);
                        SOLID_CHECK(notifyConnection(impl_->manager(), rrcvcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
                    } else {
                        vdbgx(Debug::mpipc, _rrelay_con_uid << " rcv con " << rrcvcon.id_ << " not notified for message " << _rengine_msg_id);
                    }
                }
            } else {
                vdbgx(Debug::mpipc, _rrelay_con_uid << " simply erase the message " << _rengine_msg_id);
                //simply release the message
                rsndcon.send_msg_list_.erase(msgidx);
                rmsg.clear();
                impl_->eraseMessage(_rengine_msg_id.index);
            }

            SOLID_ASSERT(_prelay_data == nullptr);
            return;
        }
        if (rmsg.receiver_con_id_.isValid()) {
            SOLID_ASSERT(rmsg.receiver_con_id_ == _rrelay_con_uid);

            ConnectionStub& rrcvcon = impl_->con_dq_[rmsg.receiver_con_id_.index];
            //cancel comes from receiving connection
            //_prelay_data, if not empty, contains the last relay data on the message
            //which should return to the sending connection

            rrcvcon.recv_msg_list_.erase(msgidx);
            rmsg.receiver_con_id_.clear(); //unlink from receiver connection
            rmsg.state_ = MessageStateE::RecvCancel;

            if (rmsg.sender_con_id_.isValid()) {
                SOLID_ASSERT(rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique);

                ConnectionStub& rsndcon                  = impl_->con_dq_[rmsg.sender_con_id_.index];
                bool            should_notify_connection = rsndcon.pdone_relay_data_top_ == nullptr;

                if (_prelay_data) {
                    _prelay_data->pnext_          = rsndcon.pdone_relay_data_top_;
                    rsndcon.pdone_relay_data_top_ = _prelay_data;
                    _prelay_data                  = nullptr;
                }

                should_notify_connection = should_notify_connection || (msgidx == rsndcon.send_msg_list_.frontIndex() || rsndcon.send_msg_list_.front().state_ != MessageStateE::RecvCancel);

                rsndcon.send_msg_list_.erase(msgidx);
                rsndcon.send_msg_list_.pushFront(msgidx);
                SOLID_ASSERT(rsndcon.send_msg_list_.check());

                if (should_notify_connection) {
                    idbgx(Debug::mpipc, _rrelay_con_uid << " notify sending connection of canceled message " << _rengine_msg_id);
                    SOLID_CHECK(notifyConnection(impl_->manager(), rsndcon.id_, RelayEngineNotification::DoneData), "Connection should be alive");
                } else {
                    vdbgx(Debug::mpipc, _rrelay_con_uid << " snd con not notified for message " << _rengine_msg_id);
                }
            } else {
                vdbgx(Debug::mpipc, _rrelay_con_uid << " simply erase the message " << _rengine_msg_id);
                //simply release the message
                rmsg.clear();
                impl_->eraseMessage(_rengine_msg_id.index);
            }
        }
    } else {
        edbgx(Debug::mpipc, _rrelay_con_uid << " message not found " << _rengine_msg_id);
    }

    if (_prelay_data) {
        SOLID_ASSERT(not _prelay_data->bufptr_);
        _prelay_data->clear();
        impl_->eraseRelayData(_prelay_data);
    }
}
//-----------------------------------------------------------------------------
void EngineCore::doRegisterConnectionId(const ConnectionContext& _rconctx, const size_t _idx)
{
    ConnectionStub& rcon = impl_->con_dq_[_idx];
    rcon.id_             = _rconctx.connectionId();
    _rconctx.relayId(UniqueId(_idx, rcon.unique_));
}
//-----------------------------------------------------------------------------
void EngineCore::debugDump()
{
    unique_lock<mutex> lock(impl_->mtx_);
    int                i = 0;
    for (const auto& msg : impl_->msg_dq_) {
        size_t datacnt = 0;
        {
            auto p = msg.pfront_;
            while (p) {
                ++datacnt;
                p = p->pnext_;
            }
        }
        edbgx(Debug::mpipc, "Msg " << (i++) << ": state " << (int)msg.state_ << " datacnt = " << datacnt << " hasData = " << msg.hasData() << " rcvcon = " << msg.receiver_con_id_ << " sndcon " << msg.sender_con_id_);
    }
    i = 0;
    for (const auto& con : impl_->con_dq_) {
        edbgx(Debug::mpipc, "Con " << i++ << ": " << plot(con) << " done = " << con.pdone_relay_data_top_ << " rcvlst = " << con.recv_msg_list_ << " sndlst = " << con.send_msg_list_);
    }
}
//-----------------------------------------------------------------------------
size_t EngineCore::Proxy::createConnection()
{
    return re_.impl_->createConnection();
}
//-----------------------------------------------------------------------------
ConnectionStubBase& EngineCore::Proxy::connection(const size_t _idx)
{
    return re_.impl_->con_dq_[_idx];
}
//-----------------------------------------------------------------------------
bool EngineCore::Proxy::notifyConnection(const ObjectIdT& _rrelay_con_uid, const RelayEngineNotification _what)
{
    return re_.notifyConnection(re_.manager(), _rrelay_con_uid, _what);
}
//-----------------------------------------------------------------------------
void EngineCore::Proxy::stopConnection(const size_t _idx)
{
    re_.doStopConnection(_idx);
}
//-----------------------------------------------------------------------------
void EngineCore::Proxy::registerConnectionId(const ConnectionContext& _rconctx, const size_t _idx)
{
    re_.doRegisterConnectionId(_rconctx, _idx);
}
//-----------------------------------------------------------------------------
} //namespace relay
} //namespace mpipc
} //namespace frame
} //namespace solid
