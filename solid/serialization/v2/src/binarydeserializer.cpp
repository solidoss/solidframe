
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

void DeserializerBase::addBasic(bool& _rb, const char* _name)
{
    idbg(_name);
    Runnable r{&_rb, &load_bool, 1, 0, _name};

    if (isRunEmpty()) {
        if (load_bool(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

void DeserializerBase::addBasic(int8_t& _rb, const char* _name)
{
    idbg(_name);
    Runnable r{&_rb, &load_byte, 1, 0, _name};

    if (isRunEmpty()) {
        if (load_byte(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

void DeserializerBase::addBasic(uint8_t& _rb, const char* _name)
{
    idbg(_name);
    Runnable r{&_rb, &load_byte, 1, 0, _name};

    if (isRunEmpty()) {
        if (load_byte(*this, r, nullptr) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

void DeserializerBase::addBasic(std::string& _rb, const char* _name)
{
    idbg(_name);
    bool init   = true;
    auto lambda = [init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
        if (init) {
            init      = false;
            _rr.data_ = _rd.sentinel();
            SOLID_ASSERT(_rd.isRunEmpty());
            _rd.addBasic(_rr.size_, _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty(); //isRunEmpty depends on the current sentinel

            _rd.sentinel(_rr.data_);

            if (not is_run_empty) {
                return ReturnE::Wait;
            }
        }

        //at this point _rr.size_ contains the size of the string
        std::string& rs = *static_cast<std::string*>(_rr.ptr_);
        rs.resize(_rr.size_);

        _rr.ptr_  = const_cast<char*>(rs.data());
        _rr.data_ = 0;
        _rr.call_ = load_binary;

        return load_binary(_rd, _rr, _pctx);
    };

    Runnable r{&_rb, call_function, lambda, _name};

    tryRun(std::move(r));
}

void DeserializerBase::limits(const Limits &_rlimits){
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
            }, ""
            };
        schedule(std::move(r));
    }
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

Base::ReturnE DeserializerBase::load_byte(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rd.pcrt_ != _rd.pend_) {
        uint8_t* pb = static_cast<uint8_t*>(_rr.ptr_);
        *pb         = *reinterpret_cast<const uint8_t*>(_rd.pcrt_);
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
        _rr.ptr_ = reinterpret_cast<uint8_t*>(_rr.ptr_) + toread;
        return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
    }
    return ReturnE::Wait;
}

Base::ReturnE DeserializerBase::call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rr.fnc_(_rd, _rr, _pctx);
}

Base::ReturnE DeserializerBase::noop(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return ReturnE::Done;
}

Base::ReturnE DeserializerBase::load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    //we can only use _rd.buf_ and _rr.size_ - the length will be stored in _rr.size_

    size_t len = _rd.pend_ - _rd.pcrt_;
    if (len > _rr.size_) {
        len = _rr.size_;
    }

    memcpy(_rd.buf_ + _rr.data_, _rd.pcrt_, len);

    _rr.size_ -= len;
    _rr.data_ += len;
    _rd.pcrt_ += len;

    if (_rr.size_ == 0) {
        uint16_t v;
        load(_rd.buf_, v);
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
