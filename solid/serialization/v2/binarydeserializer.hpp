#pragma once

#include "solid/serialization/v2/binarybase.hpp"
#include "solid/serialization/v2/binarybasic.hpp"
#include "solid/serialization/v2/typemapbase.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/innerlist.hpp"
#include <istream>
#include <list>
#include <ostream>

namespace solid {
namespace serialization {
namespace v2 {
namespace binary {

class DeserializerBase : public Base {
    struct Runnable;

    typedef ReturnE (*CallbackT)(DeserializerBase&, Runnable&, void*);

    using FunctionT = SOLID_FUNCTION(ReturnE(DeserializerBase&, Runnable&, void*));

    struct Runnable {
        Runnable(
            void*       _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
        {
        }

        template <class F>
        Runnable(void* _ptr, CallbackT _call, F _f, const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(0)
            , data_(0)
            , name_(_name)
            , fnc_(std::move(_f))
        {
        }

        template <class F>
        Runnable(
            void*       _ptr,
            CallbackT   _call,
            uint64_t    _size,
            uint64_t    _data,
            F&&         _f,
            const char* _name)
            : ptr_(_ptr)
            , call_(_call)
            , size_(_size)
            , data_(_data)
            , name_(_name)
            , fnc_(std::move(_f))
        {
        }

        void clear()
        {
            ptr_  = nullptr;
            call_ = nullptr;
            SOLID_FUNCTION_CLEAR(fnc_);
        }

        void*       ptr_;
        CallbackT   call_;
        uint64_t    size_;
        uint64_t    data_;
        const char* name_;
        FunctionT   fnc_;
    };

    using RunListT         = std::list<Runnable>;
    using RunListIteratorT = std::list<Runnable>::const_iterator;

protected:
    DeserializerBase(const TypeMapBase& _rtype_map, const Limits& _rlimits);
    DeserializerBase(const TypeMapBase& _rtype_map);

public:
    static constexpr bool is_serializer   = false;
    static constexpr bool is_deserializer = true;

public:
    std::istream& run(std::istream& _ris, void* _pctx = nullptr);
    long          run(const char* _pbeg, unsigned _sz, void* _pctx = nullptr);

    void clear();

    void limits(const Limits& _rlimits);

    const Limits& limits() const
    {
        return Base::limits();
    }
    const ErrorConditionT& error() const
    {
        return Base::error();
    }

    bool empty() const
    {
        return run_lst_.empty();
    }

    inline void addBasic(bool& _rb, const char* _name)
    {
        idbg(_name);
        Runnable r{&_rb, &load_bool, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadBool(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(int8_t& _rb, const char* _name)
    {
        idbg(_name);
        Runnable r{&_rb, &load_byte, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    inline void addBasic(uint8_t& _rb, const char* _name)
    {
        idbg(_name);
        Runnable r{&_rb, &load_byte, 1, 0, _name};

        if (isRunEmpty()) {
            if (doLoadByte(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class T>
    void addBasic(T& _rb, const char* _name)
    {
        idbg(_name);
#if 0
        Runnable r{&_rb, &load_binary, sizeof(T), 0, _name};

        if (isRunEmpty()) {
            if (doLoadBinary(r) == ReturnE::Done) {
                return;
            }
        }
#elif 1
        Runnable r{&_rb, &load_cross<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (doLoadCross<T>(r) == ReturnE::Done) {
                return;
            }
        }
#else
        Runnable r{&_rb, &load_cross_with_check<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (load_cross_with_check<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }
#endif
        schedule(std::move(r));
    }

    template <class T>
    void addBasicWithCheck(T& _rb, const char* _name)
    {
        idbg(_name);

        Runnable r{&_rb, &load_cross_with_check<T>, 0, 0, _name};

        if (isRunEmpty()) {
            if (load_cross_with_check<T>(*this, r, nullptr) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    void addBasic(std::string& _rb, const char* _name)
    {
        idbg(_name);
        Runnable r{&_rb, &load_string, 0, 0, _name};

        if (isRunEmpty()) {
            if (doLoadString(r) == ReturnE::Done) {
                return;
            }
        }

        schedule(std::move(r));
    }

    template <class D, class F>
    void addFunction(D& _rd, F&& _f, const char* _name)
    {
        idbg(_name);
        if (isRunEmpty()) {
            _f(_rd, _name);
        } else {
            Runnable r{
                call_function,
                [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable {
                    const RunListIteratorT old_sentinel = _rd.sentinel();

            _f(static_cast<D&>(_rd), _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);
            if (is_run_empty) {
                return Base::ReturnE::Done;
            } else {
                _rr.call_ = noop;
                return Base::ReturnE::Wait;
            }
        }
        ,
            _name
    };
    schedule(std::move(r));
}
} // namespace binary

template <class D, class F, class Ctx>
void addFunction(D& _rd, F _f, Ctx& _rctx, const char* _name)
{
    idbg(_name);
    if (isRunEmpty()) {
        _f(_rd, _rctx, _name);
    } else {
        Runnable r{
            nullptr,
            call_function,
            [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable {
                const RunListIteratorT old_sentinel = _rd.sentinel();

        _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);
        if (is_run_empty) {
            return Base::ReturnE::Done;
        } else {
            _rr.call_ = noop;
            return Base::ReturnE::Wait;
        }
    }
    ,
        _name
};
schedule(std::move(r));
} // namespace v2
} // namespace serialization

template <class D, class F>
void pushFunction(D& _rd, F _f, const char* _name)
{
    idbg(_name);
    auto lambda = [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
    {
        const RunListIteratorT old_sentinel = _rd.sentinel();
        const bool             done         = _f(static_cast<D&>(_rd), _rr.name_);

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);

        if (done) {
            _rr.call_ = noop;
        }
        if (is_run_empty) {
            if (done) {
                return Base::ReturnE::Done;
            } else {
                return Base::ReturnE::Continue;
            }
        } else {
            if (done) {
                _rr.call_ = noop;
            }
            return Base::ReturnE::Wait;
        }
    };
    Runnable r{nullptr, call_function, lambda, _name};

    tryRun(std::move(r));
}

template <class D, class F, class Ctx>
void pushFunction(D& _rd, F&& _f, Ctx& _rctx, const char* _name)
{
    idbg(_name);
    auto lambda = [_f = std::move(_f)](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
    {
        const RunListIteratorT old_sentinel = _rd.sentinel();
        const bool             done         = _f(static_cast<D&>(_rd), *static_cast<Ctx*>(_pctx), _rr.name_);

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);

        if (done) {
            _rr.call_ = noop;
        }
        if (is_run_empty) {
            if (done) {
                return Base::ReturnE::Done;
            } else {
                return Base::ReturnE::Continue;
            }
        } else {
            if (done) {
                _rr.call_ = noop;
            }
            return Base::ReturnE::Wait;
        }
    };
    Runnable r{nullptr, call_function, lambda, _name};

    tryRun(std::move(r), &_rctx);
}

template <class D, class C>
void addContainer(D& _rd, C& _rc, const char* _name)
{
    idbg(_name);

    typename C::value_type value;
    bool                   init          = true;
    bool                   parsing_value = false;
    auto                   lambda        = [value, parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
        C& rcontainer = *static_cast<C*>(_rr.ptr_);
        D& rd         = static_cast<D&>(_rd);

        if (init) {
            init                                = false;
            const RunListIteratorT old_sentinel = _rd.sentinel();
            SOLID_ASSERT(_rd.isRunEmpty());

            rd.addBasicWithCheck(_rr.size_, _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);
            if (not is_run_empty) {
                return ReturnE::Wait;
            }
        }

        if (parsing_value) {
            rcontainer.insert(rcontainer.end(), value);
            parsing_value = false;
        } else if (_rd.limits().hasContainer() && _rr.size_ > _rd.limits().container()) {
            _rd.error(error_limit_stream);
            return ReturnE::Done;
        }

        const RunListIteratorT old_sentinel = _rd.sentinel();

        while (_rd.pcrt_ != _rd.pend_ and _rr.size_ != 0) {
            rd.add(value, _rr.name_);
            --_rr.size_;

            if (_rd.isRunEmpty()) {
                //the value was parsed
                rcontainer.insert(rcontainer.end(), value);
            } else {
                parsing_value = true;
                SOLID_CHECK(_rd.pcrt_ == _rd.pend_, "buffer not empty");
            }
        }

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);

        if (_rr.size_ == 0 and is_run_empty) {
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    };

    Runnable r{&_rc, call_function, lambda, _name};

    tryRun(std::move(r));
}

template <class D, class C, class Ctx>
void addContainer(D& _rd, C& _rc, Ctx& _rctx, const char* _name)
{
    idbg(_name);

    typename C::value_type value;
    bool                   init          = true;
    bool                   parsing_value = false;
    auto                   lambda        = [value, parsing_value, init](DeserializerBase& _rd, Runnable& _rr, void* _pctx) mutable {
        C&   rcontainer = *static_cast<C*>(_rr.ptr_);
        D&   rd         = static_cast<D&>(_rd);
        Ctx& rctx       = *static_cast<Ctx*>(_pctx);

        if (init) {
            init                                = false;
            const RunListIteratorT old_sentinel = _rd.sentinel();
            SOLID_ASSERT(_rd.isRunEmpty());

            rd.addBasicWithCheck(_rr.size_, _rr.name_);

            const bool is_run_empty = _rd.isRunEmpty();
            _rd.sentinel(old_sentinel);
            if (not is_run_empty) {
                return ReturnE::Wait;
            }
        }

        if (parsing_value) {
            rcontainer.insert(rcontainer.end(), value);
            parsing_value = false;
        } else if (_rd.limits().hasContainer() && _rr.size_ > _rd.limits().container()) {
            _rd.error(error_limit_stream);
            return ReturnE::Done;
        }

        const RunListIteratorT old_sentinel = _rd.sentinel();

        while (_rd.pcrt_ != _rd.pend_ and _rr.size_ != 0) {
            rd.add(value, rctx, _rr.name_);
            --_rr.size_;

            if (_rd.isRunEmpty()) {
                //the value was parsed
                rcontainer.insert(rcontainer.end(), value);
            } else {
                parsing_value = true;
                SOLID_CHECK(_rd.pcrt_ == _rd.pend_, "buffer not empty");
            }
        }

        const bool is_run_empty = _rd.isRunEmpty();
        _rd.sentinel(old_sentinel);

        if (_rr.size_ == 0 and is_run_empty) {
            return ReturnE::Done;
        }
        return ReturnE::Wait;
    };

    Runnable r{&_rc, call_function, lambda, _name};

    tryRun(std::move(r), &_rctx);
}

template <class F, class Ctx>
void addStream(std::ostream& _ros, F _f, Ctx& _rctx, const char* _name)
{
    uint64_t len    = 0;
    auto     lambda = [ _f = std::move(_f), len ](DeserializerBase & _rd, Runnable & _rr, void* _pctx) mutable
    {
        Ctx&          rctx = *static_cast<Ctx*>(_pctx);
        std::ostream& ros  = *const_cast<std::ostream*>(static_cast<const std::ostream*>(_rr.ptr_));
        len += _rr.data_;

        _f(ros, len, _rr.data_ == 0, rctx, _rr.name_);

        if (_rd.limits().hasStream() && len > _rd.limits().stream()) {
            _rd.error(error_limit_stream);
        }
        return ReturnE::Done;
    };

    Runnable r{&_ros, &load_stream, 0, 0, lambda, _name};
    if (isRunEmpty()) {
        if (load_stream(*this, r, &_rctx) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

void addBinary(void* _pv, const size_t _sz, const char* _name)
{
    Runnable r{_pv, &load_binary, _sz, 0, _name};

    if (isRunEmpty()) {
        if (doLoadBinary(r) == ReturnE::Done) {
            return;
        }
    }

    schedule(std::move(r));
}

protected:
void doPrepareRun(const char* _pbeg, unsigned _sz)
{
    pbeg_ = _pbeg;
    pend_ = _pbeg + _sz;
    pcrt_ = _pbeg;
}
long doRun(void* _pctx = nullptr);

private:
friend class TypeMapBase;
void tryRun(Runnable&& _ur, void* _pctx = nullptr);

RunListIteratorT sentinel()
{
    RunListIteratorT old = sentinel_;
    sentinel_            = run_lst_.cbegin();
    return old;
}

void sentinel(const RunListIteratorT _s)
{
    sentinel_ = _s;
}

bool isRunEmpty() const
{
    return sentinel_ == run_lst_.cbegin();
}

RunListIteratorT schedule(Runnable&& _ur)
{

    return run_lst_.emplace(sentinel_, std::move(_ur));
}

void error(const ErrorConditionT& _err)
{
    if (!error_) {
        error_ = _err;
        pcrt_ = pbeg_ = pend_ = nullptr;
    }
}

static ReturnE load_bool(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE load_byte(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE load_binary(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE call_function(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

template <typename T>
static ReturnE load_cross_data(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadCrossData<T>(_rr);
}

template <typename T>
inline static ReturnE load_cross(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    return _rd.doLoadCross<T>(_rr);
}

template <typename T>
static ReturnE load_cross_with_check(DeserializerBase& _rd, Runnable& _rr, void* _pctx)
{
    if (_rd.pcrt_ != _rd.pend_) {
        if (_rr.size_ == 0) {
            //first run
            uint64_t    v;
            const char* p = cross::load_with_check(_rd.pcrt_, _rd.pend_ - _rd.pcrt_, v);

            if (p) {
                _rd.pcrt_ = p;

                T vt = static_cast<T>(v);

                if (static_cast<uint64_t>(vt) == v) {
                    *reinterpret_cast<T*>(_rr.ptr_) = vt;
                } else {
                    _rd.error(error_cross_integer);
                }

                return ReturnE::Done;
            } else {
                //not enough data
                _rr.size_ = cross::size(_rd.pcrt_);

                if (_rr.size_ == InvalidSize()) {
                    _rd.error(error_cross_integer);
                    return ReturnE::Done;
                }

                size_t toread = _rd.pend_ - _rd.pcrt_;
                SOLID_CHECK(toread < _rr.size_, "Should not happen");
                memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
                _rd.pcrt_ += toread;
                _rr.size_ -= toread;
                _rr.data_ += toread;
                return ReturnE::Wait;
            }
        } else {
            size_t toread = _rd.pend_ - _rd.pcrt_;
            SOLID_CHECK(toread >= _rr.size_, "Should not happen");
            if (toread > _rr.size_) {
                toread = _rr.size_;
            }
            memcpy(_rd.data_.buf_ + _rr.data_, _rd.pcrt_, toread);
            _rd.pcrt_ += toread;
            _rr.size_ -= toread;
            _rr.data_ += toread;
            if (_rr.size_ == 0) {
                uint64_t    v;
                T           vt;
                const char* p = cross::load_with_check(_rd.data_.buf_, _rr.data_, v);
                if (p == nullptr) {
                    _rd.error(error_cross_integer);
                    return ReturnE::Done;
                }

                vt = static_cast<T>(v);

                if (static_cast<uint64_t>(vt) == v) {
                    *reinterpret_cast<T*>(_rr.ptr_) = vt;
                } else {
                    _rd.error(error_cross_integer);
                }

                return ReturnE::Done;
            }
            return ReturnE::Wait;
        }
    }
    return ReturnE::Wait;
}

static ReturnE load_string(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

static ReturnE noop(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

static ReturnE load_stream(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE load_stream_chunk_length(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE load_stream_chunk_begin(DeserializerBase& _rd, Runnable& _rr, void* _pctx);
static ReturnE load_stream_chunk(DeserializerBase& _rd, Runnable& _rr, void* _pctx);

private:
inline Base::ReturnE doLoadBinary(Runnable& _rr)
{
    if (pcrt_ != pend_) {
        size_t toread = pend_ - pcrt_;
        if (toread > _rr.size_) {
            toread = _rr.size_;
        }
        memcpy(_rr.ptr_, pcrt_, toread);
        pcrt_ += toread;
        _rr.size_ -= toread;
        _rr.ptr_ = reinterpret_cast<uint8_t*>(_rr.ptr_) + toread;
        return _rr.size_ == 0 ? ReturnE::Done : ReturnE::Wait;
    }
    return ReturnE::Wait;
}
inline Base::ReturnE doLoadBool(Runnable& _rr)
{
    if (pcrt_ != pend_) {
        bool* pb = static_cast<bool*>(_rr.ptr_);
        *pb      = *reinterpret_cast<const uint8_t*>(pcrt_) == 0xFF ? true : false;
        ++pcrt_;
        return ReturnE::Done;
    }
    return ReturnE::Wait;
}

inline Base::ReturnE doLoadByte(Runnable& _rr)
{
    if (pcrt_ != pend_) {
        uint8_t* pb = static_cast<uint8_t*>(_rr.ptr_);
        *pb         = *reinterpret_cast<const uint8_t*>(pcrt_);
        ++pcrt_;
        return ReturnE::Done;
    }
    return ReturnE::Wait;
}
inline Base::ReturnE doLoadString(Runnable& _rr)
{
    idbg(_rr.name_);
    //_rr.ptr_ contains pointer to string object
    void* pstr      = _rr.ptr_;
    _rr.ptr_        = &data_.u64_;
    const ReturnE r = load_cross_with_check<uint64_t>(*this, _rr, nullptr);
    _rr.ptr_        = pstr;

    if (r == ReturnE::Done and data_.u64_ != 0) {
        _rr.size_ = data_.u64_;
        idbg("size = " << _rr.size_);

        if (limits().hasString() && _rr.size_ > limits().string()) {
            error(error_limit_string);
            return ReturnE::Done;
        }

        std::string& rstr = *static_cast<std::string*>(pstr);
        rstr.resize(_rr.size_);
        _rr.ptr_  = const_cast<char*>(rstr.data());
        _rr.data_ = 0;
        _rr.call_ = load_binary;
        return doLoadBinary(_rr);
    }
    return r;
}

template <typename T>
inline ReturnE doLoadCrossData(Runnable& _rr)
{
    if (pcrt_ != pend_) {
        size_t toread = pend_ - pcrt_;
        if (toread > _rr.size_) {
            toread = _rr.size_;
        }

        memcpy(data_.buf_ + _rr.data_, pcrt_, toread);

        _rr.size_ -= toread;
        _rr.data_ += toread;
        pcrt_ += toread;

        if (_rr.size_ == 0) {
            const uint64_t v  = data_.u64_;
            const T        vt = static_cast<T>(v);
            idbg("vt = " << vt);

            if (static_cast<uint64_t>(vt) == v) {
                *reinterpret_cast<T*>(_rr.ptr_) = vt;
            } else {
                error(error_cross_integer);
            }
            return ReturnE::Done;
        }
    }
    return ReturnE::Wait;
}

template <typename T>
inline ReturnE doLoadCross(Runnable& _rr)
{
    if (pcrt_ != pend_) {
        _rr.size_ = *pcrt_;
        idbg("sz = " << _rr.size_ << " c = " << (int)*pcrt_);
        ++pcrt_;

        if (_rr.size_ > sizeof(uint64_t)) {
            SOLID_ASSERT(false);
            error(error_cross_integer);
            return ReturnE::Done;
        } else if (_rr.size_ == 0) {
            *reinterpret_cast<T*>(_rr.ptr_) = 0;
            return ReturnE::Done;
        }
        _rr.call_  = load_cross_data<T>;
        data_.u64_ = 0;
        return doLoadCrossData<T>(_rr);
    }
    return ReturnE::Wait;
}

protected:
enum {
    BufferCapacityE = sizeof(uint64_t) * 1
};
const TypeMapBase& rtype_map_;
union {
    char     buf_[BufferCapacityE];
    uint64_t u64_;
    void*    p_;
} data_;

private:
const char*      pbeg_;
const char*      pend_;
const char*      pcrt_;
RunListT         run_lst_;
RunListIteratorT sentinel_;
}; // namespace solid

template <typename TypeId, class Ctx = void>
class Deserializer;

template <typename TypeId>
class Deserializer<TypeId, void> : public DeserializerBase {
    friend class TypeMapBase;
    TypeId type_id_;

public:
    using ThisT = Deserializer<TypeId, void>;

    explicit Deserializer(const TypeMapBase& _rtype_map, const Limits& _rlimits)
        : DeserializerBase(_rtype_map, _rlimits)
    {
    }
    explicit Deserializer(const TypeMapBase& _rtype_map)
        : DeserializerBase(_rtype_map)
    {
    }

    template <typename F>
    ThisT& add(std::ostream& _ros, F _f, const char* _name)
    {
        addStream(_ros, _f, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(T& _rt, const char* _name)
    {
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(const T& _rt, const char* _name)
    {
        solidSerializeV2(*this, _rt, _name);
        return *this;
    }

    ThisT& add(void* _pv, size_t _sz, const char* _name)
    {
        addBinary(_pv, _sz, _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T& _rt, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T&& _rt, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _name);
        return *this;
    }

    ThisT& limits(const Limits& _rlimits)
    {
        DeserializerBase::limits(_rlimits);
        return *this;
    }

    template <typename F>
    long run(const char* _pbeg, unsigned _sz, F _f)
    {
        doPrepareRun(_pbeg, _sz);
        _f(*this);
        return doRun();
    }

    template <typename F>
    std::istream& run(std::istream& _ris, F _f)
    {
        const size_t buf_cap = 8 * 1024;
        char         buf[buf_cap];

        clear();
        _ris.read(buf, buf_cap);

        if (_ris.gcount()) {
            doPrepareRun(buf, _ris.gcount());

            _f(*this);

            if (_ris.gcount() == doRun()) {
                do {
                    _ris.read(buf, buf_cap);
                } while (_ris.gcount() and (_ris.gcount() == DeserializerBase::run(buf, _ris.gcount())));
            }
        }

        return _ris;
    }

    template <class T>
    void addPointer(std::shared_ptr<T>& _rp, const char* _name)
    {
        idbg(_name);
        add(type_id_, _name);
        add([&_rp](ThisT& _rd, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _name);
        },
            _name);
    }

    template <class T, class D>
    void addPointer(std::unique_ptr<T, D>& _rp, const char* _name)
    {
        idbg(_name);
        add(type_id_, _name);
        add([&_rp](ThisT& _rd, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _name);
        },
            _name);
    }
};

template <typename TypeId, class Ctx>
class Deserializer : public DeserializerBase {
    friend class TypeMapBase;
    TypeId type_id_;

public:
    using ThisT    = Deserializer<TypeId, Ctx>;
    using ContextT = Ctx;

    explicit Deserializer(const TypeMapBase& _rtype_map, const Limits& _rlimits)
        : DeserializerBase(_rtype_map, _rlimits)
    {
    }
    explicit Deserializer(const TypeMapBase& _rtype_map)
        : DeserializerBase(_rtype_map)
    {
    }

    template <typename F>
    ThisT& add(std::ostream& _ros, F _f, Ctx& _rctx, const char* _name)
    {
        addStream(_ros, _f, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& add(const T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializeV2(*this, _rt, _rctx, _name);
        return *this;
    }

    ThisT& add(void* _pv, size_t _sz, const char* _name)
    {
        addBinary(_pv, _sz, _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _rctx, _name);
        return *this;
    }

    template <typename T>
    ThisT& push(T&& _rt, Ctx& _rctx, const char* _name)
    {
        solidSerializePushV2(*this, std::move(_rt), _rctx, _name);
        return *this;
    }

    ThisT& limits(const Limits& _rlimits)
    {
        DeserializerBase::limits(_rlimits);
        return *this;
    }

    std::istream& run(std::istream& _ris, Ctx& _rctx)
    {
        return DeserializerBase::run(_ris, &_rctx);
    }

    std::pair<ThisT&, Ctx&> wrap(Ctx& _rct)
    {
        return std::make_pair(std::ref(*this), std::ref(_rct));
    }

    template <typename F>
    long run(const char* _pbeg, unsigned _sz, F _f, Ctx& _rctx)
    {
        doPrepareRun(_pbeg, _sz);
        _f(*this, _rctx);
        return doRun(&_rctx);
    }

    long run(const char* _pbeg, unsigned _sz, Ctx& _rctx)
    {
        return DeserializerBase::run(_pbeg, _sz, &_rctx);
    }

    template <typename F>
    std::istream& run(std::istream& _ris, F _f, Ctx& _rctx)
    {
        const size_t buf_cap = 8 * 1024;
        char         buf[buf_cap];

        clear();
        _ris.read(buf, buf_cap);

        if (_ris.gcount()) {
            doPrepareRun(buf, _ris.gcount());

            _f(*this, _rctx);

            if (_ris.gcount() == doRun(&_rctx)) {
                do {
                    _ris.read(buf, buf_cap);
                } while (_ris.gcount() and (_ris.gcount() == DeserializerBase::run(buf, _ris.gcount(), &_rctx)));
            }
        }

        return _ris;
    }

    const ErrorConditionT& error() const
    {
        return Base::error();
    }

    template <class T>
    void addPointer(std::shared_ptr<T>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        add(type_id_, _rctx, _name);
        add([&_rp](ThisT& _rd, Ctx& _rctx, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _rctx, _name);
        },
            _rctx, _name);
    }

    template <class T, class D>
    void addPointer(std::unique_ptr<T, D>& _rp, Ctx& _rctx, const char* _name)
    {
        idbg(_name);
        add(type_id_, _rctx, _name);
        add([&_rp](ThisT& _rd, Ctx& _rctx, const char* _name) mutable {
            _rd.rtype_map_.deserialize(_rd, _rp, _rd.type_id_, _rctx, _name);
        },
            _rctx, _name);
    }
};

template <typename TypeId>
inline std::istream& operator>>(std::istream& _ris, Deserializer<TypeId>& _rdes)
{
    return _rdes.DeserializerBase::run(_ris);
}

template <typename D>
inline std::istream& operator>>(std::istream& _ris, std::pair<D&, typename D::ContextT&> _des)
{
    return _des.first.run(_ris, _des.second);
}

} // namespace binary
} // namespace v2
} // namespace serialization
} // namespace solid
