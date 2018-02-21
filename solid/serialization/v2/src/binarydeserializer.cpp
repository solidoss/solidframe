
#include "solid/serialization/v2/binarydeserializer.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/system/exception.hpp"

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

//== Serializer  ==============================================================
DeserializerBase::DeserializerBase()
    : pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(InvalidIndex())
    , run_lst_(run_vec_)
    , cache_lst_(run_vec_)
{
}

std::istream& DeserializerBase::run(std::istream& _ris, void* _pctx)
{
    const size_t buf_cap = 8 * 1024;
    char         buf[buf_cap];

    do {
        _ris.read(buf, buf_cap);
    } while (_ris.gcount() and (_ris.gcount() == run(buf, _ris.gcount())));

    return _ris;
}

long DeserializerBase::run(const char* _pbeg, unsigned _sz, void* _pctx)
{
    pbeg_ = _pbeg;
    pend_ = _pbeg + _sz;
    pcrt_ = _pbeg;

    while (not run_lst_.empty()) {
        Runnable&     rr = run_lst_.front();
        const ReturnE rv = rr.call_(*this, rr, _pctx);
        switch (rv) {
        case ReturnE::Done:
            rr.clear();
            cache_lst_.pushBack(run_lst_.popFront());
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
    run_vec_.clear();
}

size_t DeserializerBase::schedule(Runnable&& _ur)
{
    size_t idx;
    if (cache_lst_.size()) {
        idx           = cache_lst_.popFront();
        run_vec_[idx] = std::move(_ur);
    } else {
        idx = run_vec_.size();
        run_vec_.emplace_back(std::move(_ur));
    }

    if (sentinel_ == InvalidIndex()) {
        run_lst_.pushBack(idx);
    } else {
        run_lst_.insertFront(sentinel_, idx);
    }
    return idx;
}

void DeserializerBase::tryRun(Runnable&& _ur, void* _pctx)
{
    size_t idx = schedule(std::move(_ur));

    if (idx == run_lst_.frontIndex()) {
        //we try run the function on spot
        Runnable& rr = run_lst_.front();
        ReturnE   v  = rr.call_(*this, rr, _pctx);
        if (v == ReturnE::Done) {
            rr.clear();
            cache_lst_.pushBack(run_lst_.popFront());
        }
    }
}

void DeserializerBase::limits(const Limits& _rlimits)
{
    idbg("");
    if (isRunEmpty()) {
        limits_ = _rlimits;
    } else {
        Runnable r{
            nullptr,
            call_function,
            [_rlimits](DeserializerBase& _rd, Runnable& /*_rr*/, void* /*_pctx*/) {
                _rd.limits_ = _rlimits;
                return Base::ReturnE::Done;
            },
            ""};
        schedule(std::move(r));
    }
}

//-- load functions ----------------------------------------------------------

Base::ReturnE DeserializerBase::load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadBool(_rr);
}

Base::ReturnE DeserializerBase::load_byte(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadByte(_rr);
}

Base::ReturnE DeserializerBase::load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadBinary(_rr);
}

Base::ReturnE DeserializerBase::call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rr.fnc_(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::noop(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return ReturnE::Done;
}

Base::ReturnE DeserializerBase::load_string(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadString(_rr);
}

Base::ReturnE DeserializerBase::load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    //we can only use _rd.buf_ and _rr.size_ - the length will be stored in _rr.size_

    size_t len = _rd.pend_ - _rd.pcrt_;
    if (len > _rr.size_) {
        len = _rr.size_;
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
    } else {
        _rr.size_ = 2;
        _rr.data_ = 0;
        _rr.call_ = load_stream_chunk_length;
        return load_stream_chunk_length(_rd, _rr, _pctx);
    }
}

Base::ReturnE DeserializerBase::load_stream_chunk_begin(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rr.size_ == 0) {
        _rr.data_ = 0;
        _rr.fnc_(_rd, _rr, _pctx);
        return ReturnE::Done;
    } else {
        _rr.call_ = load_stream_chunk;
        return load_stream_chunk(_rd, _rr, _pctx);
    }
}

Base::ReturnE DeserializerBase::load_stream_chunk(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    std::ostream& ros = *const_cast<std::ostream*>(static_cast<const std::ostream*>(_rr.ptr_));
    size_t        len = _rd.pend_ - _rd.pcrt_;
    if (len) {
        if (len > _rr.size_) {
            len = _rr.size_;
        }

        ros.write(_rd.pcrt_, len);

        _rr.size_ -= len;
        _rd.pcrt_ += len;

        _rr.data_ = len;
        _rr.fnc_(_rd, _rr, _pctx);

        if (_rd.error()) {
            return ReturnE::Done;
        }

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
