// solid/serialization/v2/src/binarydeserializer.cpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v2/binarydeserializer.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/system/exception.hpp"

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

//== Deserializer  ==============================================================
DeserializerBase::DeserializerBase(const TypeMapBase& _rtype_map, const Limits& _rlimits)
    : Base(_rlimits)
    , rtype_map_(_rtype_map)
    , pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(run_lst_.cend())
{
}

DeserializerBase::DeserializerBase(const TypeMapBase& _rtype_map)
    : rtype_map_(_rtype_map)
    , pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(run_lst_.cend())
{
}
std::istream& DeserializerBase::run(std::istream& _ris, void* /*_pctx*/)
{
    const size_t    buf_cap = 8 * 1024;
    char            buf[buf_cap];
    std::streamsize readsz;
    clear();

    do {
        _ris.read(buf, buf_cap);
        readsz = _ris.gcount();
    } while (readsz != 0 && (readsz == run(buf, static_cast<unsigned>(readsz))));

    return _ris;
}

long DeserializerBase::run(const char* _pbeg, unsigned _sz, void* _pctx)
{
    doPrepareRun(_pbeg, _sz);
    return doRun(_pctx);
}

long DeserializerBase::doRun(void* _pctx)
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
    long rv = error_ ? -1 : pcrt_ - pbeg_;
    pcrt_ = pbeg_ = pend_ = nullptr;
    return rv;
}

void DeserializerBase::clear()
{
    run_lst_.clear();
    error_ = ErrorConditionT();
    limits_.clear();
}

void DeserializerBase::tryRun(Runnable&& _ur, void* _pctx)
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

//-- load functions ----------------------------------------------------------

Base::ReturnE DeserializerBase::load_bool(DeserializerBase& _rd, Runnable& _rr, void* /*_pctx*/)
{
    return _rd.doLoadBool(_rr);
}

Base::ReturnE DeserializerBase::load_byte(DeserializerBase& _rd, Runnable& _rr, void* /*_pctx*/)
{
    return _rd.doLoadByte(_rr);
}

Base::ReturnE DeserializerBase::load_binary(DeserializerBase& _rd, Runnable& _rr, void* /*_pctx*/)
{
    return _rd.doLoadBinary(_rr);
}

Base::ReturnE DeserializerBase::load_version(DeserializerBase& _rd, Runnable& _rr, void* /*_pctx*/)
{
    const uint32_t& rv = *reinterpret_cast<uint32_t*>(_rr.ptr_);
    _rd.doAddVersion(_rr.data_, rv);
    return ReturnE::Done;
}

Base::ReturnE DeserializerBase::call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rr.fnc_(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::noop(DeserializerBase& /*_rd*/, Runnable& /*_rr*/, void* /*_pctx*/)
{
    return ReturnE::Done;
}

Base::ReturnE DeserializerBase::load_string(DeserializerBase& _rd, Runnable& _rr, void* /*_pctx*/)
{
    return _rd.doLoadString(_rr);
}

Base::ReturnE DeserializerBase::load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    //we can only use _rd.buf_ and _rr.size_ - the length will be stored in _rr.size_

    size_t len = _rd.pend_ - _rd.pcrt_;
    if (len > _rr.size_) {
        len = static_cast<size_t>(_rr.size_);
    }

    memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, len);

    _rr.size_ -= len;
    _rr.data_ += len;
    _rd.pcrt_ += len;

    if (_rr.size_ == 0) {
        uint16_t v;
        load(_rd.data_.buf_, v);
        _rr.size_ = v;
        _rr.call_ = load_stream_chunk_begin;
        return load_stream_chunk_begin(_rd, _rr, _pctx);
    }

    return ReturnE::Wait;
}

Base::ReturnE DeserializerBase::load_stream(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    ptrdiff_t len = _rd.pend_ - _rd.pcrt_;
    if (len >= 2) {
        uint16_t v;
        _rd.pcrt_ = load(_rd.pcrt_, v);
        _rr.size_ = v;
        _rr.call_ = load_stream_chunk_begin;
        return load_stream_chunk_begin(_rd, _rr, _pctx);
    }

    _rr.size_ = 2;
    _rr.data_ = 0;
    _rr.call_ = load_stream_chunk_length;
    return load_stream_chunk_length(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::load_stream_chunk_begin(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rr.size_ == 0) {
        _rr.data_ = 0;
        _rr.fnc_(_rd, _rr, _pctx);
        return ReturnE::Done;
    }
    _rr.call_ = load_stream_chunk;
    return load_stream_chunk(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::load_stream_chunk(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    std::ostream& ros = *const_cast<std::ostream*>(static_cast<const std::ostream*>(_rr.ptr_));
    size_t        len = _rd.pend_ - _rd.pcrt_;
    if (len != 0u) {
        if (len > _rr.size_) {
            len = static_cast<size_t>(_rr.size_);
        }

        _rr.data_ = len;

        _rr.fnc_(_rd, _rr, _pctx);

        if (_rd.error()) {
            return ReturnE::Done;
        }

        ros.write(_rd.pcrt_, len);

        _rd.pcrt_ += len;
        _rr.size_ -= len;

        if (_rr.size_ == 0) {
            _rr.call_ = load_stream;
            return load_stream(_rd, _rr, _pctx);
        }
    }
    return ReturnE::Wait;
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
