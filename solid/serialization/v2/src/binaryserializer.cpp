
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/binaryserializer.hpp"
#include "solid/system/exception.hpp"

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

//== Serializer  ==============================================================
SerializerBase::SerializerBase()
    : pbeg_(nullptr)
    , pend_(nullptr)
    , pcrt_(nullptr)
    , sentinel_(InvalidIndex())
    , run_lst_(run_vec_)
    , cache_lst_(run_vec_)
{
}

std::ostream& SerializerBase::run(std::ostream& _ros)
{
    const size_t buf_cap = 8 * 1024;
    char         buf[buf_cap];
    long         len;

    while ((len = run(buf, buf_cap)) > 0) {
        _ros.write(buf, len);
    }
    return _ros;
}

long SerializerBase::run(char* _pbeg, unsigned _sz, void* _pctx)
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

size_t SerializerBase::schedule(Runnable&& _ur)
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
    return idx;
}

void SerializerBase::addBasic(const bool& _rb, const char* _name)
{
    idbg("");
    Runnable r{&_rb, &store_bool, 1, 0, _name};

    if (isRunEmpty()) {
        if (store_bool(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

void SerializerBase::addBasic(const int32_t& _rb, const char* _name)
{
    idbg("");
    Runnable r{nullptr, &store_cross, 0, static_cast<uint64_t>(_rb), _name};
    if (isRunEmpty()) {
        if (store_cross(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}
void SerializerBase::addBasic(const uint64_t& _rb, const char* _name)
{
    idbg("");
    Runnable r{nullptr, &store_cross, 0, static_cast<uint64_t>(_rb), _name};
    if (isRunEmpty()) {
        if (store_cross(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

//-- store functions ----------------------------------------------------------

Base::ReturnE SerializerBase::store_bool(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    if (_rs.pcrt_ != _rs.pend_) {
        const bool* pb = static_cast<const bool*>(_rr.ptr_);
        *_rs.pcrt_     = (*pb) ? 0xFF : 0xAA;
        ++_rs.pcrt_;
        return ReturnE::Done;
    }
    return ReturnE::Wait;
}

Base::ReturnE SerializerBase::store_cross(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    if (_rs.pcrt_ != _rs.pend_) {
        if (_rr.ptr_ == nullptr) {
            //first run
            char* p = cross::store(_rs.pcrt_, _rs.pend_ - _rs.pcrt_, _rr.data_);
            if (p != nullptr) {
                _rs.pcrt_ = p;
                return ReturnE::Done;
            } else {
                p = cross::store(_rs.buf_, BufferCapacityE, _rr.data_);
                SOLID_CHECK(p, "should not be null");
                _rr.ptr_  = _rs.buf_;
                _rr.size_ = p - _rs.buf_;
                _rr.data_ = 0;
                _rr.call_ = store_binary;
                return store_binary(_rs, _rr, _pctx);
            }
        }
    }
    return ReturnE::Wait;
}

Base::ReturnE SerializerBase::store_binary(SerializerBase& _rs, Runnable& _rr, void* _pctx)
{
    if (_rs.pcrt_ != _rs.pend_) {
        size_t towrite = _rs.pend_ - _rs.pcrt_;
        if (towrite > _rr.size_) {
            towrite = _rr.size_;
        }
        memcpy(_rs.pcrt_, _rr.ptr_, towrite);
        _rs.pcrt_ += towrite;
        _rr.size_ -= towrite;
        return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
    }
    return ReturnE::Wait;
}

Base::ReturnE SerializerBase::store_function(SerializerBase& _rs, Runnable& _rr, void* _pctx){
    return _rr.fnc_(_rs, _rr, _pctx);
}

} //namespace binary
} //namespace v2
} //namespace serialization
} //namespace solid
