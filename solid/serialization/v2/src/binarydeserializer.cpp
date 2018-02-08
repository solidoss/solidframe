
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/binarydeserializer.hpp"
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

    while(_ris.read(buf, buf_cap) and _ris.gcount() != run(buf, _ris.gcount())){
    }
    return _ris;
}

long DeserializerBase::run(const char* _pbeg, unsigned _sz, void* _pctx)
{
    pbeg_ = _pbeg;
    pend_ = _pbeg + _sz;
    pcrt_ = _pbeg;

    while (not run_lst_.empty()) {
        const ReturnE rv = run_lst_.front().call_(*this, run_lst_.front(), _pctx);
        switch (rv) {
        case ReturnE::Done:
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

void DeserializerBase::schedule(Runnable&& _ur)
{
    size_t idx;
    if(cache_lst_.size()){
        idx = cache_lst_.popFront();
        run_vec_[idx] = std::move(_ur);
    }else{
        idx = run_vec_.size();
        run_vec_.emplace_back(std::move(_ur));
    }
    
    if(sentinel_ == InvalidIndex()){
        run_lst_.pushBack(idx);
    }else{
        run_lst_.insertBefore(sentinel_, idx);
    }
}

void DeserializerBase::addBasic(bool& _rb, const char* _name)
{
    idbg("");
    Runnable r{&_rb, &load_bool, 1, 0, _name};

    if (run_lst_.empty()) {
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
        *pb = *reinterpret_cast<const uint8_t*>(_rd.pcrt_) == 0xFF ? true : false;
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

Base::ReturnE DeserializerBase::load_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx){
    return _rr.fnc_(_rd, _rr, _pctx);
}


} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid

