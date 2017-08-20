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
    ObjectIdT id_;
    string    name_;
};

struct MessageStub {
    RelayData*    pfront_;
    RelayData*    pback_;
    uint32_t      unique_;
    ObjectIdT     sender_id_;
    MessageHeader header_;

    MessageStub()
        : pfront_(nullptr)
        , pback_(nullptr)
        , unique_(0)
    {
    }
    void clear()
    {
        pfront_ = nullptr;
        pback_  = nullptr;
        ++unique_;
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

using MessageDequeT   = std::deque<MessageStub>;
using RelayDataDequeT = std::deque<RelayData>;
using RelayDataStackT = std::stack<RelayData*>;
using SizeTStackT     = std::stack<size_t>;
} //namespace

struct RelayEngine::Data {
    mutex           mtx_;
    MessageDequeT   msg_dq_;
    RelayDataDequeT reldata_dq_;
    RelayDataStackT reldata_cache_;
    SizeTStackT     msg_cache_;

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
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname)
{
}
//-----------------------------------------------------------------------------
bool RelayEngine::relay(
    const ObjectIdT& _rconuid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    MessageId&       _rrelay_id,
    ErrorConditionT& _rerror)
{
    size_t msgidx;

    unique_lock<mutex> lock(impl_->mtx_);

    if (_rrelay_id.isValid()) {
        if (_rrelay_id.index < impl_->msg_dq_.size() and impl_->msg_dq_[_rrelay_id.index].unique_ == _rrelay_id.unique) {
            msgidx = _rrelay_id.index;
        } else {
            return false;
        }
    } else {
        //TODO: try allocate connection

        msgidx            = impl_->createMessage();
        MessageStub& rmsg = impl_->msg_dq_[msgidx];
        rmsg.header_      = std::move(_rmsghdr);
        _rrelay_id.index  = msgidx;
        _rrelay_id.unique = rmsg.unique_;
        rmsg.sender_id_   = _rconuid;
    }
    MessageStub& rmsg = impl_->msg_dq_[msgidx];
    rmsg.push(impl_->createRelayData(std::move(_rrelmsg)));

    return true;
}
//-----------------------------------------------------------------------------
void RelayEngine::doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    //TODO:
}
//-----------------------------------------------------------------------------
void RelayEngine::doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, RelayData* _prelay_data, MessageId const& _rengine_msg_id, bool& _rmore)
{
    //TODO:
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
