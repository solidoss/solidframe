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
struct ConnectionStub {
    uint32_t   unique_;
    ObjectIdT  id_;
    string     name_;
    size_t     front_msg_idx_;
    size_t     back_msg_idx_;
    RelayData* pdone_relay_data_top_;

    ConnectionStub()
        : unique_(0)
        , front_msg_idx_(InvalidIndex())
        , back_msg_idx_(InvalidIndex())
        , pdone_relay_data_top_(nullptr)
    {
    }
    ConnectionStub(std::string&& _uname)
        : unique_(0)
        , name_(std::move(_uname))
        , front_msg_idx_(InvalidIndex())
        , back_msg_idx_(InvalidIndex())
        , pdone_relay_data_top_(nullptr)
    {
    }

    void clear()
    {
        ++unique_;
        id_.clear();
        name_.clear();
        front_msg_idx_ = back_msg_idx_ = InvalidIndex();
        pdone_relay_data_top_          = nullptr;
    }
};

struct MessageStub {
    RelayData*    pfront_;
    RelayData*    pback_;
    uint32_t      unique_;
    ObjectIdT     sender_con_id_;
    ObjectIdT     receiver_con_id_;
    MessageId     receiver_msg_id_;
    size_t        next_msg_idx_;
    MessageHeader header_;

    MessageStub()
        : pfront_(nullptr)
        , pback_(nullptr)
        , unique_(0)
        , next_msg_idx_(InvalidIndex())
    {
    }
    void clear()
    {
        pfront_ = nullptr;
        pback_  = nullptr;
        ++unique_;
        next_msg_idx_ = InvalidIndex();
        sender_con_id_.clear();
        receiver_con_id_.clear();
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
void RelayEngine::connectionStop(const ObjectIdT& _rconuid)
{
    unique_lock<mutex> lock(impl_->mtx_);

    const auto it = impl_->con_id_umap_.find(_rconuid.index);
    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);
        RelayData* prd = rcon.pdone_relay_data_top_;

        idbgx(Debug::mpipc, _rconuid << " name = " << rcon.name_ << " con_idx = " << con_idx);

        while (prd) {
            RelayData* ptmprd = prd->pnext_;
            prd->clear();
            impl_->reldata_cache_.push(prd);
            prd = ptmprd;
        }

        impl_->con_umap_.erase(rcon.name_.c_str());
        impl_->con_id_umap_.erase(rcon.id_.index);

        rcon.clear();
        impl_->con_cache_.push(_rconuid.index);
    } else {
        edbgx(Debug::mpipc, "connection not registered: " << _rconuid);
    }
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname)
{
    to_lower(_uname);
    unique_lock<mutex> lock(impl_->mtx_);

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
    impl_->con_dq_[con_idx].id_         = _rconuid;
    impl_->con_id_umap_[_rconuid.index] = con_idx;

    idbgx(Debug::mpipc, _rconuid << " name = " << impl_->con_dq_[con_idx].name_ << " con_idx = " << con_idx);
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
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
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

        const auto it = impl_->con_id_umap_.find(_rconuid.index);
        if (it == impl_->con_id_umap_.end()) {
            //TODO: maybe set _rerror
            edbgx(Debug::mpipc, "relay must be called only on registered connections");
            return false;
        }

        const size_t    con_idx = doRegisterConnection(std::move(_rmsghdr.url_));
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];
        msgidx                  = impl_->createMessage();
        MessageStub& rmsg       = impl_->msg_dq_[msgidx];
        rmsg.header_            = std::move(_rmsghdr);
        _rrelay_id.index        = msgidx;
        _rrelay_id.unique       = rmsg.unique_;
        rmsg.next_msg_idx_      = InvalidIndex();

        //also hold the in-engine connection id into msg
        rmsg.sender_con_id_   = ObjectIdT(it->first, impl_->con_dq_[it->first].unique_);
        rmsg.receiver_con_id_ = ObjectIdT(con_idx, rcon.unique_);
    }

    MessageStub&    rmsg = impl_->msg_dq_[msgidx];
    ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];

    idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_con_idx " << rmsg.receiver_con_id_.index << " sender_con_idx " << rmsg.sender_con_id_.index);

    SOLID_ASSERT(rmsg.pfront_ == nullptr);

    rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

    if (rcon.back_msg_idx_ == InvalidIndex()) {
        rcon.front_msg_idx_ = rcon.back_msg_idx_ = msgidx;
        if (not _rnotify_fnc(rcon.id_, RelayEngineNotification::NewData)) {
            //TODO:
            SOLID_ASSERT(false);
        }
    } else {
        impl_->msg_dq_[rcon.back_msg_idx_].next_msg_idx_ = msgidx;
        rcon.back_msg_idx_                               = msgidx;
    }
    return true;
}
//-----------------------------------------------------------------------------
bool RelayEngine::doRelayResponse(
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    const MessageId& _rrelay_id,
    ErrorConditionT& _rerror)
{
    unique_lock<mutex> lock(impl_->mtx_);
    SOLID_ASSERT(_rrelay_id.isValid());

    if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
        const size_t msgidx = _rrelay_id.index;
        MessageStub& rmsg   = impl_->msg_dq_[msgidx];

        idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_con_idx " << rmsg.receiver_con_id_.index << " sender_con_idx " << rmsg.sender_con_id_.index);

        std::swap(rmsg.receiver_con_id_, rmsg.sender_con_id_);
        rmsg.receiver_msg_id_.clear();

        SOLID_ASSERT(rmsg.pfront_ == nullptr);
        rmsg.header_ = std::move(_rmsghdr);

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];

        if (rcon.back_msg_idx_ == InvalidIndex()) {
            rcon.front_msg_idx_ = rcon.back_msg_idx_ = msgidx;
            if (not _rnotify_fnc(rcon.id_, RelayEngineNotification::NewData)) {
                //TODO:
                SOLID_ASSERT(false);
            }
        } else {
            impl_->msg_dq_[rcon.back_msg_idx_].next_msg_idx_ = msgidx;
            rcon.back_msg_idx_                               = msgidx;
        }

        return true;
    } else {
        edbgx(Debug::mpipc, "message not found");
        return false;
    }
}
//-----------------------------------------------------------------------------
bool RelayEngine::doRelay(
    const ObjectIdT& _rconuid,
    NotifyFunctionT& _rnotify_fnc,
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

        idbgx(Debug::mpipc, _rconuid << " msgid = " << _rrelay_id << " receiver_con_idx " << rmsg.receiver_con_id_.index << " sender_con_idx " << rmsg.sender_con_id_.index);

        rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

        if (is_msg_relay_data_queue_empty) {
            ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];

            if (rcon.back_msg_idx_ == InvalidIndex()) {
                rcon.front_msg_idx_ = rcon.back_msg_idx_ = msgidx;
                if (not _rnotify_fnc(rcon.id_, RelayEngineNotification::NewData)) {
                    //TODO:
                    SOLID_ASSERT(false);
                }
            } else {
                impl_->msg_dq_[rcon.back_msg_idx_].next_msg_idx_ = msgidx;
                rcon.back_msg_idx_                               = msgidx;
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

    const auto it = impl_->con_id_umap_.find(_rconuid.index);
    if (it != impl_->con_id_umap_.end()) {
        const size_t    con_idx = it->second;
        ConnectionStub& rcon    = impl_->con_dq_[con_idx];

        idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);
        SOLID_ASSERT(rcon.id_.unique == _rconuid.unique);

        bool   can_retry = true;
        size_t sentinel  = rcon.back_msg_idx_;

        while (can_retry and rcon.front_msg_idx_ != InvalidIndex()) {
            MessageStub& rmsg  = impl_->msg_dq_[rcon.front_msg_idx_];
            RelayData*   pnext = rmsg.pfront_->pnext_;

            edbgx(Debug::mpipc, _rconuid << " msgidx = " << rcon.front_msg_idx_ << " pnext = " << pnext);

            if (_try_push_fnc(rmsg.pfront_, MessageId(rcon.front_msg_idx_, rmsg.unique_), rmsg.receiver_msg_id_, can_retry)) {
                //relay data accepted
                rmsg.pfront_ = pnext;

                if (rmsg.pfront_ == nullptr) {
                    rmsg.pback_ = nullptr;
                }
                //drop the message from the connection queue
                rcon.front_msg_idx_ = rmsg.next_msg_idx_;
                if (rcon.front_msg_idx_ == InvalidIndex()) {
                    rcon.back_msg_idx_ = rcon.front_msg_idx_;
                }
            } else if (can_retry) {
                //relay data not accepted but we can try with other message
                //reschedule the message onto the connection's message queue
                size_t crt_msg_idx                               = rcon.front_msg_idx_;
                rcon.front_msg_idx_                              = rmsg.next_msg_idx_;
                impl_->msg_dq_[rcon.back_msg_idx_].next_msg_idx_ = crt_msg_idx;
                rmsg.next_msg_idx_                               = InvalidIndex();
                rcon.back_msg_idx_                               = crt_msg_idx;
                if (crt_msg_idx == sentinel) {
                    break;
                }
            } else {
                break;
            }
        }

        _rmore = (rcon.front_msg_idx_ != InvalidIndex());
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
void RelayEngine::doComplete(const ObjectIdT& _rconuid, NotifyFunctionT& _rnotify_fnc, RelayData* _prelay_data, MessageId const& _rengine_msg_id, bool& _rmore)
{
    unique_lock<mutex> lock(impl_->mtx_);

    if (_rengine_msg_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rengine_msg_id.index].unique_ == _rengine_msg_id.unique) {
        MessageStub& rmsg                             = impl_->msg_dq_[_rengine_msg_id.index];
        bool         receiver_connection_disconnected = false;

        if (rmsg.pfront_ != nullptr and rmsg.receiver_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.receiver_con_id_.index].unique_ == rmsg.receiver_con_id_.unique) {
            //there are pending messages to be sent
            ConnectionStub& rcon = impl_->con_dq_[rmsg.receiver_con_id_.index];

            idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

            if (rcon.back_msg_idx_ == InvalidIndex()) {
                rcon.front_msg_idx_ = rcon.back_msg_idx_ = _rengine_msg_id.index;
                if (not _rnotify_fnc(rcon.id_, RelayEngineNotification::NewData)) {
                    //TODO:
                    SOLID_ASSERT(false);
                }
            } else {
                impl_->msg_dq_[rcon.back_msg_idx_].next_msg_idx_ = _rengine_msg_id.index;
                rcon.back_msg_idx_                               = _rengine_msg_id.index;
            }
        } else {
            receiver_connection_disconnected = true;
            edbgx(Debug::mpipc, _rconuid << " received connection not found " << rmsg.receiver_con_id_.index);
        }

        if (rmsg.sender_con_id_.index < impl_->con_dq_.size() and impl_->con_dq_[rmsg.sender_con_id_.index].unique_ == rmsg.sender_con_id_.unique) {
            ConnectionStub& rcon = impl_->con_dq_[rmsg.sender_con_id_.index];

            idbgx(Debug::mpipc, _rconuid << " rcon.name = " << rcon.name_);

            bool was_pdone_relay_data_stack_empty = rcon.pdone_relay_data_top_ == nullptr;

            if (_prelay_data) {
                _prelay_data->pnext_ = rcon.pdone_relay_data_top_;

                rcon.pdone_relay_data_top_ = _prelay_data;
                if (was_pdone_relay_data_stack_empty) {
                    if (not _rnotify_fnc(rcon.id_, RelayEngineNotification::DoneData)) {
                        //TODO:
                        SOLID_ASSERT(false);
                    }
                }

                if (_prelay_data->is_last_ and not Message::is_waiting_response(_prelay_data->pmessage_header_->flags_)) {
                    rmsg.clear();
                    impl_->msg_cache_.push(_rengine_msg_id.index);
                }
            } else {
                rmsg.clear();
                impl_->msg_cache_.push(_rengine_msg_id.index);
            }
        } else if (receiver_connection_disconnected) {
            edbgx(Debug::mpipc, _rconuid << " also sender connection not found " << rmsg.sender_con_id_.index);
            //both receiver and sender connections are disconnected, erase the message
            rmsg.clear();
            impl_->msg_cache_.push(_rengine_msg_id.index);
        } else {
            edbgx(Debug::mpipc, _rconuid << " sender connection not found " << rmsg.sender_con_id_.index);
        }
    } else {
        _prelay_data->clear();
        impl_->reldata_cache_.push(_prelay_data);
        edbgx(Debug::mpipc, _rconuid << " message not found " << _rengine_msg_id);
    }
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
