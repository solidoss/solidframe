
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
    const size_t buf_cap = 11; //8 * 1024;
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
    long rv = pcrt_ - pbeg_;
    pcrt_ = pbeg_ = pend_ = nullptr;
    return rv;
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

void DeserializerBase::addBasic(bool& _rb, const char* _name)
{
    idbg("");
    Runnable r{&_rb, &load_bool, 1, 0, _name};

    if (isRunEmpty()) {
        if (load_bool(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

//-- load functions ----------------------------------------------------------

Base::ReturnE DeserializerBase::load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rd.pcrt_ != _rd.pend_) {
        bool* pb = static_cast<bool*>(_rr.ptr_);
        *pb      = *reinterpret_cast<const uint8_t*>(_rd.pcrt_) == 0xFF ? true : false;
        ++_rd.pcrt_;
        return ReturnE::Done;
    }
    return ReturnE::Wait;
}

Base::ReturnE DeserializerBase::load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rd.pcrt_ != _rd.pend_) {
        size_t toread = _rd.pend_ - _rd.pcrt_;
        if (toread > _rr.size_) {
            toread = _rr.size_;
        }
        memcpy(_rr.ptr_, _rd.pcrt_, toread);
        _rd.pcrt_ += toread;
        _rr.size_ -= toread;
        return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
    }
    return ReturnE::Wait;
}

Base::ReturnE DeserializerBase::call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rr.fnc_(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::remove_sentinel(DeserializerBase& _rs, Runnable& _rr, void* _pctx)
{
    _rs.sentinel(_rr.data_);
    return ReturnE::Done;
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
