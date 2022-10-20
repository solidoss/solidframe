// solid/serialization/v3/src/binaryserializer.cpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v3/binaryserializer.hpp"
#include "solid/serialization/v3/binarybasic.hpp"
#include "solid/system/exception.hpp"

namespace solid {
namespace serialization {
namespace v3 {
namespace binary {

//== Serializer  ==============================================================
SerializerBase::SerializerBase(const reflection::v1::TypeMapBase* const _ptype_map, const EndianessE _endianess)
    : ptype_map_(_ptype_map)
    , swap_endianess_bites_(_endianess != EndianessE::Native)
    , pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(run_lst_.cend())
{
}

std::ostream& SerializerBase::run(std::ostream& _ros, void* _pctx)
{
    constexpr size_t buf_cap = 8 * 1024;
    char             buf[buf_cap];
    ptrdiff_t        len;

    clear();

    while ((len = run(buf, buf_cap, _pctx)) > 0) {
        _ros.write(buf, len);
    }
    return _ros;
}

ptrdiff_t SerializerBase::run(char* _pbeg, unsigned _sz, void* _pctx)
{
    doPrepareRun(_pbeg, _sz);
    return doRun(_pctx);
}

ptrdiff_t SerializerBase::doRun(void* _pctx)
{
    while (!run_lst_.empty()) {
        Runnable&     rr = run_lst_.front();
        const ReturnE rv = rr.call_(*this, rr, _pctx);
        switch (rv) {
        case ReturnE::Done:
            run_lst_.pop_front();
            break;
        case ReturnE::Continue:
            break;
        case ReturnE::Wait:
            goto DONE;
        }
    }
DONE:
    ptrdiff_t rv = error_ ? -1 : pcrt_ - pbeg_;
    pcrt_ = pbeg_ = pend_ = nullptr;
    return rv;
}

void SerializerBase::clear()
{
    run_lst_.clear();
    error_ = ErrorConditionT();
}

void SerializerBase::tryRun(Runnable&& _ur, void* _pctx)
{
    const RunListIteratorT it = schedule(std::move(_ur));

    if (it == run_lst_.cbegin()) {
        // we try run the function on spot
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

Base::ReturnE SerializerBase::store_integer(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{

    return _rs.doStoreInteger(_rr);
}

Base::ReturnE SerializerBase::store_compacted(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{

    return _rs.doStoreCompacted(_rr);
}

Base::ReturnE SerializerBase::store_compacted_inline(SerializerBase& _rs, Runnable& _rr, void* /*_pctx*/)
{
    return _rs.doStoreCompactedInline(_rr);
}

Base::ReturnE SerializerBase::call_function(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    return (*_rr.fnc_ptr_)(_rs, _rr, _pctx);
}

Base::ReturnE SerializerBase::noop(SerializerBase& /*_rs*/, Runnable& /*_rr*/, void* /*_pctx*/)
{
    return ReturnE::Done;
}

Base::ReturnE SerializerBase::store_stream(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    std::istream& ris    = *const_cast<std::istream*>(static_cast<const std::istream*>(_rr.ptr_));
    size_t        toread = _rs.pend_ - _rs.pcrt_;

    if (toread <= 2) {
        return ReturnE::Wait;
    }
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

    solid_check_log(toread <= 0xffff, logger);

    _rs.pcrt_ = store(_rs.pcrt_, static_cast<uint16_t>(toread));
    _rs.pcrt_ += toread;
    _rr.data_ += toread;

    const bool done = (toread == 0); // we need to have written a final toread == 0

    if (_rr.size_ != InvalidSize()) {
        _rr.size_ -= toread;
    }

    if (!done) {
        if (_rr.size_ != 0) {
            (*_rr.fnc_ptr_)(_rs, _rr, _pctx);
            if (_rs.error()) {
                return ReturnE::Done;
            }
        }
        return ReturnE::Wait;
    }
    _rr.size_ = 0;
    (*_rr.fnc_ptr_)(_rs, _rr, _pctx);

    return ReturnE::Done;
}

} // namespace binary
} // namespace v3
} // namespace serialization
} // namespace solid
