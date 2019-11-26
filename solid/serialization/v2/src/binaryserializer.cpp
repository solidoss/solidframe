// solid/serialization/v2/src/binaryserializer.cpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v2/binaryserializer.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/system/exception.hpp"

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

//== Serializer  ==============================================================
SerializerBase::SerializerBase(const TypeMapBase& _rtype_map, const Limits& _rlimits)
    : Base(_rlimits)
    , rtype_map_(_rtype_map)
    , pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(run_lst_.cend())
{
}

SerializerBase::SerializerBase(const TypeMapBase& _rtype_map)
    : rtype_map_(_rtype_map)
    , pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(run_lst_.cend())
{
}

std::ostream& SerializerBase::run(std::ostream& _ros, void* _pctx)
{
    const size_t buf_cap = 8 * 1024;
    char         buf[buf_cap];
    long         len;

    clear();

    while ((len = run(buf, buf_cap, _pctx)) > 0) {
        _ros.write(buf, len);
    }
    return _ros;
}

long SerializerBase::run(char* _pbeg, unsigned _sz, void* _pctx)
{
    doPrepareRun(_pbeg, _sz);
    return doRun(_pctx);
}

long SerializerBase::doRun(void* _pctx)
{
    while (!run_lst_.empty()) {
        Runnable&     rr = run_lst_.front();
        const ReturnE rv = rr.call_(*this, rr, _pctx);
        switch (rv) {
        case ReturnE::Done:
            rr.clear();
            run_lst_.pop_front();
            break;
        case ReturnE::Continue:
            break;
        case ReturnE::Wait:
            goto DONE;
        }
    }
DONE:
    long rv = error_ ? -1 : pcrt_ - pbeg_;
    pcrt_ = pbeg_ = pend_ = nullptr;
    return rv;
}

void SerializerBase::clear()
{
    run_lst_.clear();
    error_ = ErrorConditionT();
    limits_.clear();
}

void SerializerBase::tryRun(Runnable&& _ur, void* _pctx)
{
    const RunListIteratorT it = schedule(std::move(_ur));

    if (it == run_lst_.cbegin()) {
        //we try run the function on spot
        Runnable& rr = run_lst_.front();
        ReturnE   v  = rr.call_(*this, rr, _pctx);
        if (v == ReturnE::Done) {
            run_lst_.pop_front();
        }
    }
}

//-- store functions ----------------------------------------------------------

Base::ReturnE SerializerBase::store_byte(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{
    return _rs.doStoreByte(_rr);
}

Base::ReturnE SerializerBase::store_cross(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{

    return _rs.doStoreCross(_rr);
}

Base::ReturnE SerializerBase::store_cross_with_check(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{
    return _rs.doStoreCrossWithCheck(_rr);
}

Base::ReturnE SerializerBase::store_binary(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{
    return _rs.doStoreBinary(_rr);
}

Base::ReturnE SerializerBase::call_function(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    return _rr.fnc_(_rs, _rr, _pctx);
}

Base::ReturnE SerializerBase::noop(SerializerBase& /*_rs*/, Runnable& /*_rr*/, void* /*_pctx*/)
{
    return ReturnE::Done;
}

Base::ReturnE SerializerBase::store_stream(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    std::istream& ris    = *const_cast<std::istream*>(static_cast<const std::istream*>(_rr.ptr_));
    size_t        toread = _rs.pend_ - _rs.pcrt_;

    if (toread > 2) {
        toread -= 2;
        if (_rr.size_ != InvalidSize()) {
            if (_rr.size_ < toread) {
                toread = static_cast<size_t>(_rr.size_);
            }
        }

        if (ris) {
            ris.read(_rs.pcrt_ + 2, toread);
            toread = static_cast<size_t>(ris.gcount());
        } else {
            toread = 0;
        }
        solid_check(toread <= 0xffff);
        uint16_t chunk_len = static_cast<uint16_t>(toread);
        _rs.pcrt_          = store(_rs.pcrt_, chunk_len);
        _rs.pcrt_ += toread;
        _rr.data_ += toread;

        if (_rs.Base::limits().hasStream() && _rr.data_ > _rs.Base::limits().stream()) {
            _rs.baseError(error_limit_stream);
            return ReturnE::Done;
        }

        bool done = (toread == 0); //we need to have written a final chunk_len == 0

        if (_rr.size_ != InvalidSize()) {
            _rr.size_ -= toread;
        }

        if (!done) {
            if (_rr.size_ != 0) {
                _rr.fnc_(_rs, _rr, _pctx);
            }
            return ReturnE::Wait;
        }
        _rr.size_ = 0;
        _rr.fnc_(_rs, _rr, _pctx);
    } else {
        return ReturnE::Wait;
    }
    return ReturnE::Done;
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
