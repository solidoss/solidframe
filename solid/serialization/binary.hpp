// solid/serialization/binary.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <bitset>
#include <istream>
#include <ostream>
#include <stack>
#include <string>
#include <typeinfo>

#include "solid/serialization/typeidmap.hpp"

#include "binarybasic.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"
#include "solid/utility/algorithm.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/stack.hpp"

namespace solid {
namespace serialization {
namespace binary {

// BASIC_DECL(int8_t);
// BASIC_DECL(uint8_t);
// BASIC_DECL(int16_t);
// BASIC_DECL(uint16_t);
// BASIC_DECL(int32_t);
// BASIC_DECL(uint32_t);
// BASIC_DECL(int64_t);
// BASIC_DECL(uint64_t);
// BASIC_DECL(bool);
// BASIC_DECL(std::string);

typedef void (*StringCheckFncT)(std::string const& /*_rstr*/, const char* /*_pb*/, size_t /*_len*/);

//! Nonintrusive string solid/serialization/deserialization specification
// template <class S>
// S& operator&(std::string &_t, S &_s){
//  return _s.push(_t, "string");
// }

template <class S, class T>
void solidSerialize(S& _s, T& _t)
{
    _t.solidSerialize(_s);
}
template <class S, class Ctx, class T>
void solidSerialize(S& _s, T& _t, Ctx& _ctx)
{
    _t.solidSerialize(_s, _ctx);
}

enum {
    MIN_STREAM_BUFFER_SIZE = 16 //if the free space for current buffer is less than this value
    //storring a stream will end up returning Wait
};

enum ReturnValues {
    ContinueE,
    SuccessE,
    WaitE,
    FailureE,
    LastReturnValueE,
};

struct Limits {
    static Limits const& unlimited_instance();

    Limits(
        size_t   _stringlimit    = InvalidSize(),
        size_t   _containerlimit = InvalidSize(),
        uint64_t _streamlimit    = InvalidSize())
        : stringlimit(_stringlimit)
        , containerlimit(_containerlimit)
        , streamlimit(_streamlimit)
    {
    } //unlimited by default

    size_t   stringlimit;
    size_t   containerlimit;
    uint64_t streamlimit;
};

struct ExtendedData {
    enum {
        MAX_GENERIC_SIZE = 256,
    };

    typedef void (*DeleteFunctionT)(void*);

    union BasicValue {
        uint8_t  uint8_t_value;
        uint32_t uint32_t_value;
        uint64_t uint64_t_value;
        int64_t  int64_t_value;
        int32_t  int32_t_value;
        void*    void_value;
    };

    BasicValue first_value_;

    struct Generic {
        size_t          type_id;
        void*           ptr;
        char            buffer[MAX_GENERIC_SIZE];
        DeleteFunctionT delete_fnc;
    } generic_;

    const uint8_t& first_uint8_t_value() const
    {
        return first_value_.uint8_t_value;
    }

    uint8_t& first_uint8_t_value()
    {
        return first_value_.uint8_t_value;
    }

    const uint32_t& first_uint32_t_value() const
    {
        return first_value_.uint32_t_value;
    }

    uint32_t& first_uint32_t_value()
    {
        return first_value_.uint32_t_value;
    }

    const uint64_t& first_uint64_t_value() const
    {
        return first_value_.uint64_t_value;
    }

    uint64_t& first_uint64_t_value()
    {
        return first_value_.uint64_t_value;
    }

    const int32_t& first_int32_t_value() const
    {
        return first_value_.int32_t_value;
    }

    int32_t& first_int32_t_value()
    {
        return first_value_.int32_t_value;
    }

    const int64_t& first_int64_t_value() const
    {
        return first_value_.int64_t_value;
    }

    int64_t& first_int64_t_value()
    {
        return first_value_.int64_t_value;
    }

    void* const& first_void_value() const
    {
        return first_value_.void_value;
    }

    void*& first_void_value()
    {
        return first_value_.void_value;
    }

    ExtendedData()
    {
        init();
    }

    explicit ExtendedData(uint8_t _u8)
    {
        init();
        first_uint8_t_value() = _u8;
    }
    explicit ExtendedData(uint32_t _u32)
    {
        init();
        first_uint32_t_value() = _u32;
    }
    explicit ExtendedData(int32_t _i32)
    {
        init();
        first_int32_t_value() = _i32;
    }
    explicit ExtendedData(uint64_t _u64)
    {
        init();
        first_uint64_t_value() = _u64;
    }

    explicit ExtendedData(void* _p)
    {
        init();
        first_void_value() = _p;
    }

    explicit ExtendedData(int64_t _i64)
    {
        init();
        first_int64_t_value() = _i64;
    }

    ~ExtendedData()
    {
        clear();
    }

    template <class T>
    static void destroyer(void* _pv)
    {
        static_cast<T*>(_pv)->~T();
    }

    template <class T>
    static void deleter(void* _pv)
    {
        delete static_cast<T*>(_pv);
    }

    template <class T>
    T* generic(const T& _rt)
    {
        clear();
        return doGenericCreate(_rt, BoolToType<sizeof(T) <= MAX_GENERIC_SIZE>());
    }

    template <class T>
    T* generic(T&& _ut)
    {
        clear();
        return doGenericCreate(std::move(_ut), BoolToType<sizeof(T) <= MAX_GENERIC_SIZE>());
    }

    template <class T>
    T* genericCast()
    {
        if (generic_.type_id == typeId<T>()) {
            return static_cast<T*>(generic_.ptr);
        } else {
            return nullptr;
        }
    }

    template <class T>
    const T* genericCast() const
    {
        if (generic_.type_id == typeId<T>()) {
            return static_cast<T*>(generic_.ptr);
        } else {
            return nullptr;
        }
    }

    void clear()
    {
        if (generic_.delete_fnc) {
            (*generic_.delete_fnc)(generic_.buffer);
            generic_.delete_fnc = nullptr;
            generic_.type_id    = 0;
            generic_.ptr        = nullptr;
            generic_.type_id    = 0;
        }
    }

private:
    template <class T>
    T* doGenericCreate(const T& _rt, BoolToType<true> /*_b*/)
    {
        T* retval           = new (generic_.buffer) T(_rt);
        generic_.ptr        = retval;
        generic_.delete_fnc = &destroyer<T>;
        generic_.type_id    = typeId<T>();
        return retval;
    }
    template <class T>
    T* doGenericCreate(const T& _rt, BoolToType<false> /*_b*/)
    {
        T* retval           = new T(_rt);
        generic_.ptr        = retval;
        generic_.delete_fnc = &deleter<T>;
        generic_.type_id    = typeId<T>();
        return retval;
    }

    template <class T>
    T* doGenericCreate(T&& _ut, BoolToType<true> /*_b*/)
    {
        T* retval           = new (generic_.buffer) T{std::move(_ut)};
        generic_.ptr        = retval;
        generic_.delete_fnc = &destroyer<T>;
        generic_.type_id    = typeId<T>();
        return retval;
    }
    template <class T>
    T* doGenericCreate(T&& _ut, BoolToType<false> /*_b*/)
    {
        T* retval           = new T{std::move(_ut)};
        generic_.ptr        = retval;
        generic_.delete_fnc = &deleter<T>;
        generic_.type_id    = typeId<T>();
        return retval;
    }

    void init()
    {
        first_value_.uint64_t_value = 0;
        generic_.delete_fnc         = nullptr;
        generic_.type_id            = 0;
    }

    static size_t newTypeId();

    template <class T>
    static size_t typeId()
    {
        static const size_t id(newTypeId());
        return id;
    }
};

#ifdef SOLID_HAS_STATISTICS
struct Statistics {
    size_t loop_count_;
    size_t push_extended_;

    Statistics()
        : loop_count_(0)
        , push_extended_(0)
    {
    }

    void onLoop()
    {
        ++loop_count_;
    }
    void onPushExtended()
    {
        ++push_extended_;
    }
};
inline std::ostream& operator<<(std::ostream& _ros, const Statistics& _rs)
{
    _ros << "Loop count: " << _rs.loop_count_ << std::endl;
    _ros << "Push extended count: " << _rs.push_extended_ << std::endl;
    return _ros;
}
#endif

template <class T>
void container_reserve_helper(std::vector<T>& _rc, size_t _cp)
{
    _rc.reserve(_cp);
}

template <class C>
void container_reserve_helper(C& _rc, size_t _cp)
{
    //do nothing
}

//===============================================================
//! A base class for binary serializer and deserializer
/*!
    The main goals for serializer and deserializer was to
    be ease to use and to be reentrant.
    The ease of use means that one should do little things to
    make a class serializable. E.g. :<br>
    <code>
    struct RemoteListCommand{<br>
    //...<br>
    template \< class S><br>
    S& operator&(S &_s){<br>
        _s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");<br>
        _s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");<br>
        _s.push(cmduid.idx, "cmduid.idx").push(cmduid.uid,"cmduid.uid");<br>
        return _s;<br>
    }<br>
    //...<br>
    //data:<br>
    RemoteList::PathListT       *ppthlst;<br>
    String                      strpth;<br>
    int                         err;<br>
    uint32_t                        tout;<br>
    fdt::ipc::ConnectionUid     conid;<br>
    fdt::ipc::CommandUid        cmduid;<br>
    uint32_t                        requid;<br>
    ObjectUidT                  fromv;<br>
    </code>
    <br>
    Reentrant means for serializer that:<br>
    * you push serializable objects onto the serializer<br>
    * you do a loop to actually serialize using a fixed size buffer
    until there is nothing to serialize:<br>
    <code>
    while((rv = ser.run(buf, blen)) == blen){<br>
        cnt += rv;<br>
        sock.write(buf, rv);<br>
    }<br>
    if(rv > 0){<br>
        sock.write(buf, blen);<br>
    }<br>
    </code>
    For deserializer means something equivalent:<br>
    * you push the serializable objects onto the deserializer<br>
    * you do a loop where you fed the deserializer, buffers filled
    e.g. from a file or a socket etc.
*/
class Base {
public:
    void resetLimits()
    {
        lmts = rdefaultlmts;
    }
    bool            ok() const { return !err; }
    ErrorConditionT error() const
    {
        return err;
    }
    ErrorConditionT streamError() const
    {
        return streamerr;
    }
    void pop()
    {
        fstk.pop();
    }
    uint64_t const& streamSize() const
    {
        return streamsz;
    }
    Limits& limits()
    {
        return lmts;
    }
#ifdef SOLID_HAS_STATISTICS
    const Statistics& statistics() const
    {
        return statistics_;
    }
#endif
protected:
    friend class ErrorCategory;
    enum Errors {
        ERR_NOERROR = 0,
        ERR_ARRAY_LIMIT,
        ERR_ARRAY_MAX_LIMIT,
        ERR_BITSET_SIZE,
        ERR_CONTAINER_LIMIT,
        ERR_CONTAINER_MAX_LIMIT,
        ERR_STREAM_LIMIT,
        ERR_STREAM_CHUNK_MAX_LIMIT,
        ERR_STREAM_SEEK,
        ERR_STREAM_READ,
        ERR_STREAM_WRITE,
        ERR_STREAM_SENDER,
        ERR_STRING_LIMIT,
        ERR_STRING_MAX_LIMIT,
        ERR_UTF8_LIMIT,
        ERR_UTF8_MAX_LIMIT,
        ERR_POINTER_UNKNOWN,
        ERR_REINIT,
        ERR_NO_TYPE_MAP,
        ERR_DESERIALIZE_VALUE,
        ERR_CROSS_VALUE_SMALL
    };
    struct FncData;
    typedef ReturnValues (*FncT)(Base&, FncData&, void*);
    //! Data associated to a callback
    struct FncData {
        FncData(
            FncT        _f,
            const void* _p,
            const char* _n = nullptr,
            uint64_t    _s = InvalidSize(),
            uint64_t    _d = 0)
            : f(_f)
            , p(const_cast<void*>(_p))
            , n(_n)
            , s(_s)
            , d(_d)
        {
        }

        FncT        f; //!< Pointer to function
        void*       p; //!< Pointer to data
        const char* n; //!< Some name - of the item serialized
        uint64_t    s; //!< Some size
        uint64_t    d; //!< Some data
    };

protected:
    static ReturnValues setStringLimit(Base& _rd, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues setStreamLimit(Base& _rd, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues setContainerLimit(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    static ReturnValues noop(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    static ErrorConditionT make_error(Errors _err);

    Base()
        : rdefaultlmts(Limits::unlimited_instance())
        , lmts(rdefaultlmts)
    {
    }

    Base(Limits const& _rdefaultlmts)
        : rdefaultlmts(_rdefaultlmts)
        , lmts(rdefaultlmts)
    {
    }
    //! Replace the top callback from the stack
    void replace(const FncData& _rfd);

    static ReturnValues popExtStack(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    static const char* default_name;

    template <typename... Args>
    void pushExtended(Args&&... _args)
    {
        estk.push(ExtendedData(std::forward<Args>(_args)...));
        SOLID_COLLECT(statistics_.onPushExtended);
    }

protected:
    typedef Stack<FncData>      FncDataStackT;
    typedef Stack<ExtendedData> ExtendedDataStackT;

    const Limits&      rdefaultlmts;
    Limits             lmts;
    ErrorConditionT    err;
    ErrorConditionT    streamerr;
    uint64_t           streamsz;
    ulong              uls;
    FncDataStackT      fstk;
    ExtendedDataStackT estk;
#ifdef SOLID_HAS_STATISTICS
    Statistics statistics_;
#endif
};

//===============================================================

template <class T>
struct SerializerPushHelper;
//! A reentrant binary serializer
/*!
    See serialization::bin::Base for details
*/
class SerializerBase : public Base {
protected:
    template <uint S>
    static ReturnValues storeBinary(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <typename T>
    static ReturnValues store(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Ser>
    static ReturnValues store(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Ser, class Ctx>
    static ReturnValues store(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    static ReturnValues storeUtf8(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    static ReturnValues storeCrossContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/);
    template <typename N>
    static ReturnValues storeCross(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <size_t V>
    static ReturnValues storeBitset(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        SerializerBase& rs(static_cast<SerializerBase&>(_rs));

        if (!rs.cpb)
            return SuccessE;

        const std::bitset<V>* pbs = reinterpret_cast<std::bitset<V>*>(_rfd.p);
        const char*           n   = _rfd.n;

        if (pbs) {
            if (pbs->size() > rs.lmts.containerlimit) {
                rs.err = make_error(ERR_CONTAINER_LIMIT);
                return FailureE;
            }

            uint64_t crcsz;

            if (!compute_value_with_crc(crcsz, pbs->size())) {
                rs.err = make_error(ERR_CONTAINER_MAX_LIMIT);
                return FailureE;
            }
            _rfd.f = &SerializerBase::storeBitsetContinue<V>;
            _rfd.s = 0;
            idbgx(Debug::ser_bin, " sz = " << crcsz);
            rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, n, 0, crcsz));
        } else {

            idbgx(Debug::ser_bin, " sz = " << -1);
            _rfd.f = &SerializerBase::template storeCross<uint64_t>;
            _rfd.d = -1;
        }
        return ContinueE;
    }

    template <size_t V>
    static ReturnValues storeBitsetContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        SerializerBase& rs(static_cast<SerializerBase&>(_rs));

        if (!rs.cpb)
            return SuccessE;

        const std::bitset<V>* pbs    = reinterpret_cast<std::bitset<V>*>(_rfd.p);
        size_t                bitoff = 0;

        while ((rs.be - rs.cpb) && _rfd.s < pbs->size()) {
            uint8_t* puc = reinterpret_cast<uint8_t*>(rs.cpb);

            if (bitoff == 0) {
                *puc = 0;
            }

            *puc |= (pbs->test(_rfd.s) ? (1 << bitoff) : 0);
            ++_rfd.s;
            ++bitoff;
            if (bitoff == 8) {
                ++rs.cpb;
                bitoff = 0;
            }
        }

        if (_rfd.s < pbs->size()) {
            return WaitE;
        } else if (bitoff) {
            ++rs.cpb;
        }

        return SuccessE;
    }

    static ReturnValues storeBitvec(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    static ReturnValues storeBitvecContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Ser>
    static ReturnValues storeContainer(Base& _rs, FncData& _rfd, void* _pctx)
    {
        idbgx(Debug::ser_bin, "store generic container sizeof(iterator) = " << sizeof(typename T::iterator) << " " << _rfd.n);
        SerializerBase& rs(static_cast<SerializerBase&>(_rs));

        if (!rs.cpb)
            return SuccessE;

        T*          c = reinterpret_cast<T*>(_rfd.p);
        const char* n = _rfd.n;

        if (c) {
            SOLID_ASSERT(sizeof(typename T::iterator) <= sizeof(ExtendedData));
            if (c->size() > rs.lmts.containerlimit) {
                rs.err = make_error(ERR_CONTAINER_LIMIT);
                return FailureE;
            }
            uint64_t crcsz;
            if (!compute_value_with_crc(crcsz, c->size())) {
                rs.err = make_error(ERR_CONTAINER_MAX_LIMIT);
                return FailureE;
            }

            rs.pushExtended();

            typename T::iterator* pit = rs.estk.top().generic(c->begin());

            FncData fd(_rfd); //we need a local copy

            _rfd.f = &Base::popExtStack;
            _rfd.p = nullptr;

            fd.f = &SerializerBase::storeContainerContinue<T, Ser>;

            rs.fstk.push(fd);

            *pit = c->begin();

            idbgx(Debug::ser_bin, " sz = " << crcsz);
            rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, n, 0, crcsz));
        } else {
            idbgx(Debug::ser_bin, " sz = " << -1);
            _rfd.f = &SerializerBase::template storeCross<uint64_t>;
            _rfd.d = -1;
        }
        return ContinueE;
    }

    template <typename T, class Ser>
    static ReturnValues storeContainerContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        using IteratorT = typename T::iterator;

        Ser&          rs       = static_cast<Ser&>(_rs);
        ExtendedData& rextdata = rs.estk.top();
        IteratorT&    rit      = *(rextdata.genericCast<IteratorT>());
        T*            c        = reinterpret_cast<T*>(_rfd.p);

        if (rs.cpb && rit != c->end()) {
            rs.push(*rit, _rfd.n);
            ++rit;
            return ContinueE;
        }
        return SuccessE;
    }

    template <typename T, class Ser>
    static ReturnValues storeArray(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        idbgx(Debug::ser_bin, "store generic array " << _rfd.n);
        SerializerBase& rs(static_cast<SerializerBase&>(_rs));
        if (!rs.cpb) {
            return SuccessE;
        }

        T*          c = reinterpret_cast<T*>(_rfd.p);
        const char* n = _rfd.n;

        if (c && _rfd.s > 0) {
            uint64_t crcsz;
            if (_rfd.s > rs.lmts.containerlimit || _rfd.s > _rfd.d) {
                rs.err = make_error(ERR_ARRAY_LIMIT);
                return FailureE;
            } else if (compute_value_with_crc(crcsz, _rfd.s)) {
                _rfd.f = &SerializerBase::storeArrayContinue<T, Ser>;
                _rfd.d = 0; //used as the current position within the array
                idbgx(Debug::ser_bin, "store array size " << crcsz);
                rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, n, 0, crcsz));
            } else {
                rs.err = make_error(ERR_ARRAY_MAX_LIMIT);
                return FailureE;
            }
        } else {
            idbgx(Debug::ser_bin, "store array size " << 0);
            _rfd.f = &SerializerBase::template storeCross<uint64_t>;
            _rfd.d = 0;
        }
        return ContinueE;
    }

    template <typename T, class Ser>
    static ReturnValues storeArrayContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        Ser& rs(static_cast<Ser&>(_rs));
        T*   c = reinterpret_cast<T*>(_rfd.p);

        idbgx(Debug::ser_bin, "store generic array cont " << _rfd.n << " rsz = " << _rfd.s << " ri = " << _rfd.d);

        if (rs.cpb && _rfd.d < _rfd.s) {
            rs.push(c[_rfd.d], _rfd.n);
            ++_rfd.d;
            return ContinueE;
        }
        return SuccessE;
    }

    static ReturnValues storeStreamBegin(Base& _rs, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues storeStreamCheck(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    //! Internal callback for storing a stream
    static ReturnValues storeStream(Base& _rs, FncData& _rfd, void* /*_pctx*/);

    template <class Ser>
    static ReturnValues storeCall(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        Ser& rs(static_cast<Ser&>(_rs));
        //const uint32_t    val = _rfd.s;

        if (!rs.cpb) {
            rs.estk.pop();
            return SuccessE;
        }

        typename Ser::CallT call = std::move(*rs.estk.top().template genericCast<typename Ser::CallT>());

        rs.estk.pop();

        call(rs, _rfd.s, rs.err);

        if (rs.err) {
            _rfd.f = storeReturnError;
        } else {
            _rfd.f = storeReturnSuccess;
        }
        return ContinueE;
    }

    template <class Ser, class Ctx>
    static ReturnValues storeCall(Base& _rs, FncData& _rfd, void* _pctx)
    {
        Ser& rs(static_cast<Ser&>(_rs));
        //const uint32_t    val = _rfd.s;

        if (!rs.cpb) {
            return SuccessE;
        }

        Ctx& rctx = *reinterpret_cast<Ctx*>(_pctx);

        typename Ser::CallT call = std::move(*rs.estk.top().template genericCast<typename Ser::CallT>());

        rs.estk.pop();

        call(rs, rctx, _rfd.s, rs.err);

        if (rs.err) {
            _rfd.f = storeReturnError;
        } else {
            _rfd.f = storeReturnSuccess;
        }
        return ContinueE;
    }

    static ReturnValues storeReturnError(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        SerializerBase& rs(static_cast<SerializerBase&>(_rs));
        if (!rs.err) {
            rs.err = make_error(static_cast<Errors>(_rfd.s));
        }
        return FailureE;
    }
    static ReturnValues storeReturnSuccess(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        return SuccessE;
    }

    void doPushStringLimit();
    void doPushStringLimit(size_t _v);
    void doPushStreamLimit();
    void doPushStreamLimit(uint64_t _v);
    void doPushContainerLimit();
    void doPushContainerLimit(size_t _v);
    int  run(char* _pb, size_t _bl, void* _pctx);

public:
    typedef void ContextT;

    enum { IsSerializer = true,
        IsDeserializer  = false };
    void clear();
    bool empty() const { return fstk.empty(); }

    static char* storeValue(char* _pd, const uint8_t _val);
    static char* storeValue(char* _pd, const uint16_t _val);
    static char* storeValue(char* _pd, const uint32_t _val);
    static char* storeValue(char* _pd, const uint64_t _val);

protected:
    SerializerBase()
        : pb(nullptr)
        , cpb(nullptr)
        , be(nullptr)
    {
        tmpstr.reserve(sizeof(ulong));
    }
    SerializerBase(
        Limits const& _rdefaultlmts)
        : Base(_rdefaultlmts)
        , pb(nullptr)
        , cpb(nullptr)
        , be(nullptr)
    {
        tmpstr.reserve(sizeof(ulong));
    }
    ~SerializerBase();

private:
    template <class T>
    friend struct SerializerPushHelper;
    friend class Base;
    char*       pb;
    char*       cpb;
    char*       be;
    std::string tmpstr;
};
//===============================================================
template <>
ReturnValues SerializerBase::storeBinary<0>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeBinary<1>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeBinary<2>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeBinary<4>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeBinary<8>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<bool>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<int8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<uint8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<int16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<uint16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<int32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<uint32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<int64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<uint64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::store<std::string>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeCross<uint8_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeCross<uint16_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeCross<uint32_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues SerializerBase::storeCross<uint64_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/);

template <typename T>
ReturnValues SerializerBase::store(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    //DUMMY - should never get here
    return FailureE;
}

template <typename T, class Ser>
ReturnValues SerializerBase::store(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    idbgx(Debug::ser_bin, "store generic non pointer");
    Ser& rs(static_cast<Ser&>(_rs));
    if (!rs.cpb)
        return SuccessE;
    T& rt = *((T*)_rfd.p);
    rs.fstk.pop();
    solidSerialize(rs, rt);
    return ContinueE;
}

template <typename T, class Ser, class Ctx>
ReturnValues SerializerBase::store(Base& _rs, FncData& _rfd, void* _pctx)
{
    idbgx(Debug::ser_bin, "store generic non pointer with context");
    Ser& rs(static_cast<Ser&>(_rs));
    if (!rs.cpb)
        return SuccessE;
    T&   rt   = *((T*)_rfd.p);
    Ctx& rctx = *reinterpret_cast<Ctx*>(_pctx);
    rs.fstk.pop();
    solidSerialize(rs, rt, rctx);
    return ContinueE;
}

template <>
struct SerializerPushHelper<bool> {
    void operator()(SerializerBase& _rs, bool& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<bool>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<int8_t> {
    void operator()(SerializerBase& _rs, int8_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int8_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<uint8_t> {
    void operator()(SerializerBase& _rs, uint8_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint8_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<uint16_t> {
    void operator()(SerializerBase& _rs, uint16_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint16_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<int16_t> {
    void operator()(SerializerBase& _rs, int16_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int16_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<uint32_t> {
    void operator()(SerializerBase& _rs, uint32_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint32_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<int32_t> {
    void operator()(SerializerBase& _rs, int32_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int32_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<uint64_t> {
    void operator()(SerializerBase& _rs, uint64_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint64_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<int64_t> {
    void operator()(SerializerBase& _rs, int64_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int64_t>, &_rv, _name));
    }
};

template <>
struct SerializerPushHelper<std::string> {
    void operator()(SerializerBase& _rs, std::string& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<std::string>, &_rv, _name));
    }
};

template <class T>
struct SerializerPushHelper {
    template <class Ser>
    void operator()(Ser& _rs, T& _rv, const char* _name)
    {
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<T, Ser>, &_rv, _name));
    }
    template <class Ser>
    void operator()(Ser& _rs, T& _rv, const char* _name, bool _b)
    {
        typedef typename Ser::ContextT ContextT;
        _rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<T, Ser, ContextT>, &_rv, _name));
    }
};

//--------------------------------------------------------------
//--------------------------------------------------------------
template <class Ctx = void>
class Serializer;
//--------------------------------------------------------------
template <>
class Serializer<void> : public SerializerBase {
public:
    using ContextT    = void;
    using SerializerT = Serializer<ContextT>;
    using BaseT       = SerializerBase;
    using TypeIdMapT  = TypeIdMapSer<SerializerT>;
    using CallT       = std::function<void(SerializerT&, uint64_t, ErrorConditionT&)>;

    Serializer(
        const TypeIdMapT* _ptypeidmap = nullptr)
        : ptypeidmap(_ptypeidmap)
    {
    }
    Serializer(
        Limits const&     _rdefaultlmts,
        const TypeIdMapT* _ptypeidmap = nullptr)
        : BaseT(_rdefaultlmts)
        , ptypeidmap(_ptypeidmap)
    {
    }

    int run(char* _pb, size_t _bl)
    {
        return SerializerBase::run(_pb, _bl, nullptr);
    }

    SerializerT& pushStringLimit()
    {
        SerializerBase::doPushStringLimit();
        return *this;
    }
    SerializerT& pushStringLimit(size_t _v)
    {
        SerializerBase::doPushStringLimit(_v);
        return *this;
    }
    SerializerT& pushStreamLimit()
    {
        SerializerBase::doPushStreamLimit();
        return *this;
    }
    SerializerT& pushStreamLimit(uint64_t _v)
    {
        SerializerBase::doPushStreamLimit(_v);
        return *this;
    }
    SerializerT& pushContainerLimit()
    {
        SerializerBase::doPushContainerLimit();
        return *this;
    }
    SerializerT& pushContainerLimit(size_t _v)
    {
        SerializerBase::doPushContainerLimit(_v);
        return *this;
    }

    template <size_t V>
    SerializerT& push(std::array<uint8_t, V>& _rarray, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _rarray.data(), _name, V));
        return *this;
    }

    template <size_t V>
    SerializerT& push(std::bitset<V>& _rbitset, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBitset<V>, &_rbitset, _name, 0));
        return *this;
    }

    SerializerT& push(std::vector<bool>& _rbitvec, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBitvec, &_rbitvec, _name, 0));
        return *this;
    }

    //! Schedule a non pointer object for serialization
    /*!
        The object is only scheduled for serialization so it must remain in memory
        up until the serialization will end.

        The given name is meaningless for binary serialization, it will be usefull for
        text oriented serialization, and I want a common interface for push, so one
        can write a single template function for serializing an object.
    */
    template <typename T>
    SerializerT& push(T& _t, const char* _name = Base::default_name)
    {
        SerializerPushHelper<T> sph;
        sph(*this, _t, _name);
        return *this;
    }

    template <typename T>
    SerializerT& push(T* _pt, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            err = ptypeidmap->store(*this, _pt, _name);
            if (err) {
                SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, 0));
            }
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T1, typename T2>
    SerializerT& push(std::pair<T1, T2>& _rpair, const char* _name = Base::default_name)
    {
        push(_rpair.second, "second");
        push(_rpair.first, "first");
        return *this;
    }

    template <typename T>
    SerializerT& push(std::shared_ptr<T>& _rptr, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            err = ptypeidmap->store(*this, _rptr.get(), _name);
            if (err) {
                SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_POINTER_UNKNOWN));
            }
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    //! Schedules a stl style container for serialization
    template <typename T>
    SerializerT& pushContainer(T& _t, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)&_t, _name));
        return *this;
    }
    //! Schedules a pointer to a stl style container for serialization
    template <typename T>
    SerializerT& pushContainer(T* _t, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)_t, _name));
        return *this;
    }

    SerializerT& pushBinary(void* _p, size_t _sz, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
        return *this;
    }

    SerializerT& pushArray(uint8_t* _p, const size_t _sz, const size_t /*_cp*/, const char* _name = Base::default_name)
    {
        uint64_t crcsz;

        if (compute_value_with_crc(crcsz, _sz)) {
            idbgx(Debug::ser_bin, "store array size " << crcsz);
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
            SerializerBase::fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, _name, 0, crcsz));
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_ARRAY_MAX_LIMIT));
        }

        return *this;
    }

    SerializerT& pushArray(int8_t* _p, const size_t _sz, const size_t /*_cp*/, const char* _name = Base::default_name)
    {
        uint64_t crcsz;

        if (compute_value_with_crc(crcsz, _sz)) {
            idbgx(Debug::ser_bin, "store array size " << crcsz);
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
            SerializerBase::fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, _name, 0, crcsz));
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_ARRAY_MAX_LIMIT));
        }

        return *this;
    }

    template <typename T>
    SerializerT& pushArray(T* _p, const size_t _sz, const size_t _cp, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_p, _name, _sz, _cp));
        return *this;
    }
#if 0
    //disabled for safety
    template <typename T>
    SerializerT& pushDynamicArray(
        T*& _rp, const size_t _sz, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_rp, _name, _sz, 0));
        return *this;
    }
#endif
    SerializerT& pushUtf8(
        const std::string& _str, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeUtf8, const_cast<char*>(_str.c_str()), _name, _str.length() + 1));
        return *this;
    }

    template <class F>
    SerializerT& pushCall(F _f, uint64_t _val, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeCall<SerializerT>, nullptr, _name, _val));
        pushExtended(0);

        SerializerBase::estk.top().generic(CallT(_f));
        return *this;
    }

    SerializerT& pushStream(
        std::istream* _ps, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, -1ULL));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, -1ULL));
        return *this;
    }
    SerializerT& pushStream(
        std::istream*   _ps,
        const uint64_t& _rfrom,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, _rlen));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, _rfrom));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamCheck, _ps, _name, _rlen));
        return *this;
    }
    SerializerT& pushStream(
        std::ostream* _ps, const char* _name = Base::default_name);
    SerializerT& pushStream(
        std::ostream*   _ps,
        const uint64_t& _rfrom,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name);

    SerializerT& pushCross(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint8_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint16_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint16_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint32_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint32_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint64_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint64_t>, nullptr, _name, 0, _rv));
        return *this;
    }

    SerializerT& pushValue(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(SerializerBase::FncData(&SerializerBase::store<uint8_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushNoop(const size_t _count, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(FncData(&Base::noop, nullptr, _name, _count));
        return *this;
    }

private:
    friend class SerializerBase;
    template <typename T>
    SerializerT& push(const T& _t, const char* _name = Base::default_name)
    {
        return push(const_cast<T&>(_t), _name);
    }

    const TypeIdMapT* ptypeidmap;
};
//--------------------------------------------------------------
template <class Ctx>
class Serializer : public SerializerBase {
public:
    using ContextT    = Ctx;
    using SerializerT = Serializer<ContextT>;
    using BaseT       = SerializerBase;
    using TypeIdMapT  = TypeIdMapSer<SerializerT>;
    using CallT       = std::function<void(SerializerT&, ContextT&, uint64_t, ErrorConditionT&)>;

    Serializer(
        const TypeIdMapT* _ptypeidmap = nullptr)
        : ptypeidmap(_ptypeidmap)
    {
    }

    Serializer(
        Limits const&     _rdefaultlmts,
        const TypeIdMapT* _ptypeidmap = nullptr)
        : BaseT(_rdefaultlmts)
        , ptypeidmap(_ptypeidmap)
    {
    }

    int run(char* _pb, size_t _bl, Ctx& _rctx)
    {
        const void* pctx = &_rctx;
        return SerializerBase::run(_pb, _bl, const_cast<void*>(pctx));
    }

    SerializerT& pushStringLimit()
    {
        SerializerBase::doPushStringLimit();
        return *this;
    }
    SerializerT& pushStringLimit(size_t _v)
    {
        SerializerBase::doPushStringLimit(_v);
        return *this;
    }
    SerializerT& pushStreamLimit()
    {
        SerializerBase::doPushStreamLimit();
        return *this;
    }
    SerializerT& pushStreamLimit(uint64_t _v)
    {
        SerializerBase::doPushStreamLimit(_v);
        return *this;
    }
    SerializerT& pushContainerLimit()
    {
        SerializerBase::doPushContainerLimit();
        return *this;
    }
    SerializerT& pushContainerLimit(size_t _v)
    {
        SerializerBase::doPushContainerLimit(_v);
        return *this;
    }

    template <size_t V>
    SerializerT& push(std::array<uint8_t, V>& _rarray, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _rarray.data(), _name, V));
        return *this;
    }

    template <size_t V>
    SerializerT& push(std::bitset<V>& _rbitset, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBitset<V>, &_rbitset, _name, 0));
        return *this;
    }

    SerializerT& push(std::vector<bool>& _rbitvec, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBitvec, &_rbitvec, _name, 0));
        return *this;
    }

    //! Schedule a non pointer object for serialization
    /*!
        The object is only scheduled for serialization so it must remain in memory
        up until the serialization will end.

        The given name is meaningless for binary serialization, it will be usefull for
        text oriented serialization, and I want a common interface for push, so one
        can write a single template function for serializing an object.
    */
    template <typename T>
    SerializerT& push(T& _t, const char* _name = Base::default_name)
    {
        //fstk.push(FncData(&SerializerBase::template store<T, SerializerT>, (void*)&_t, _name));
        SerializerPushHelper<T> sph;
        sph(*this, _t, _name, true);
        return *this;
    }

    template <typename T>
    SerializerT& push(T* _pt, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            err = ptypeidmap->store(*this, _pt, _name);
            if (err) {
                SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_POINTER_UNKNOWN));
            }
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T>
    SerializerT& push(std::shared_ptr<T>& _rptr, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            err = ptypeidmap->store(*this, _rptr.get(), _name);
            if (err) {
                SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_POINTER_UNKNOWN));
            }
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T>
    SerializerT& push(std::shared_ptr<T>& _rptr, size_t _type_id, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            err = ptypeidmap->store(*this, _rptr.get(), _type_id, _name);
            if (err) {
                SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_POINTER_UNKNOWN));
            }
        } else {
            SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, nullptr, _name, SerializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T1, typename T2>
    SerializerT& push(std::pair<T1, T2>& _rpair, const char* _name = Base::default_name)
    {
        push(_rpair.second, "second");
        push(_rpair.first, "first");
        return *this;
    }

    //! Schedules a stl style container for serialization
    template <typename T>
    SerializerT& pushContainer(T& _t, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)&_t, _name));
        return *this;
    }

    //! Schedules a pointer to a stl style container for serialization
    template <typename T>
    SerializerT& pushContainer(T* _t, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)_t, _name));
        return *this;
    }
    SerializerT& pushBinary(void* _p, size_t _sz, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
        return *this;
    }
    template <typename T>
    SerializerT& pushArray(T* _p, const size_t _sz, const size_t _cp, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_p, _name, _sz, _cp));
        return *this;
    }
#if 0
    //disabled for safety
    template <typename T>
    SerializerT& pushDynamicArray(
        T*& _rp, const size_t _sz, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_rp, _name, _sz, 0));
        return *this;
    }
#endif
    SerializerT& pushUtf8(
        const std::string& _str, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeUtf8, const_cast<char*>(_str.c_str()), _name, _str.length() + 1));
        return *this;
    }

    template <class F>
    SerializerT& pushCall(F _f, uint64_t _val, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeCall<SerializerT, Ctx>, nullptr, _name, _val));
        pushExtended(0);

        SerializerBase::estk.top().generic(CallT(_f));
        return *this;
    }

    SerializerT& pushStream(
        std::istream* _ps, const char* _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, -1ULL));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, -1ULL));
        return *this;
    }
    SerializerT& pushStream(
        std::istream*   _ps,
        const uint64_t& _rfrom,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name)
    {
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, _rlen));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, _rfrom));
        SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamCheck, _ps, _name, _rlen));
        return *this;
    }
    SerializerT& pushStream(
        std::ostream* _ps, const char* _name = Base::default_name);
    SerializerT& pushStream(
        std::ostream*   _ps,
        const uint64_t& _rfrom,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name);

    SerializerT& pushCross(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint8_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint16_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint16_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint32_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint32_t>, nullptr, _name, 0, _rv));
        return *this;
    }
    SerializerT& pushCross(const uint64_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint64_t>, nullptr, _name, 0, _rv));
        return *this;
    }

    SerializerT& pushValue(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(SerializerBase::FncData(&SerializerBase::store<uint8_t>, nullptr, _name, 0, _rv));
        return *this;
    }

private:
    friend class SerializerBase;
    template <typename T>
    SerializerT& push(const T& _t, const char* _name = Base::default_name)
    {
        return push(const_cast<T&>(_t), _name);
    }

    const TypeIdMapT* ptypeidmap;
};

//===============================================================
template <class T>
struct DeserializerPushHelper;
//! A reentrant binary deserializer
/*!
    See serialization::bin::Base for details
*/
class DeserializerBase : public Base {
protected:
    //  template <typename T>
    //  static void dynamicPointerInit(void *_pptr, void *_pt){
    //      DynamicPointer<T>   &dp = *reinterpret_cast<DynamicPointer<T>*>(_pptr);
    //      T                   *pt = reinterpret_cast<T*>(_pt);
    //      dp = pt;
    //  }

    template <typename T, typename Des>
    static ReturnValues loadPlainPointer(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rd));
        if (!rd.cpb) {
            return SuccessE;
        }
        FncData fd = _rfd;
        rd.pop();

        rd.err = rd.typeIdMap()->template loadPlainPointer<T>(rd, fd.p, rd.estk.top().first_uint64_t_value(), rd.tmpstr, fd.n);

        if (!rd.err) {
            return ContinueE;
        } else {
            return FailureE;
        }
    }

    template <typename T, typename Des>
    static ReturnValues loadPlainPointerPrepare(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rd));
        if (!rd.cpb) {
            return SuccessE;
        }

        _rfd.f = &loadPlainPointer<T, Des>;

        rd.typeIdMap()->loadTypeId(rd, rd.estk.top().first_uint64_t_value(), rd.tmpstr, _rfd.n);

        return ContinueE;
    }

    template <typename T, typename Des>
    static ReturnValues loadSharedPointer(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rd));
        if (!rd.cpb) {
            return SuccessE;
        }
        FncData fd = _rfd;
        rd.pop();

        rd.err = rd.typeIdMap()->template loadSharedPointer<T>(rd, fd.p, rd.estk.top().first_uint64_t_value(), rd.tmpstr, fd.n);

        //TODO: SPTR
        //*reinterpret_cast<P>(fd.p) = reinterpret_cast<T*>(p);

        if (!rd.err) {
            return ContinueE;
        } else {
            return FailureE;
        }
    }

    template <typename T, typename Des>
    static ReturnValues loadSharedPointerPrepare(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rd));
        if (!rd.cpb) {
            return SuccessE;
        }

        _rfd.f = &loadSharedPointer<T, Des>;

        rd.typeIdMap()->loadTypeId(rd, rd.estk.top().first_uint64_t_value(), rd.tmpstr, _rfd.n);

        return ContinueE;
    }

    template <typename T>
    static ReturnValues loadCrossDone(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

        if (!rd.cpb)
            return SuccessE;

        T& v = *reinterpret_cast<T*>(_rfd.p);

        const char* p = binary::cross::load(rd.tmpstr.data(), rd.tmpstr.size(), v);
        if (p) {
            return SuccessE;
        } else {
            rd.err = make_error(ERR_CROSS_VALUE_SMALL);
            return FailureE;
        }
    }

    static ReturnValues loadCrossContinue(Base& _rd, FncData& _rfd, void* /*_pctx*/);
    template <typename T>
    static ReturnValues loadCross(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    template <size_t V>
    static ReturnValues loadBitset(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));
        if (!rd.cpb)
            return SuccessE;
        _rfd.f = &DeserializerBase::loadBitsetBegin<V>;
        rd.pushExtended(static_cast<uint64_t>(0));
        rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64_t>, &rd.estk.top().first_uint64_t_value()));
        return ContinueE;
    }

    template <size_t V>
    static ReturnValues loadBitsetBegin(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));
        if (!rd.cpb) {
            rd.estk.pop();
            return SuccessE;
        }

        uint64_t len = rd.estk.top().first_uint64_t_value();

        if (len != InvalidSize()) {
            uint64_t crcsz;
            if (check_value_with_crc(crcsz, len)) {
                rd.estk.top().first_uint64_t_value() = crcsz;
                len                                  = crcsz;
            } else {
                rd.err = make_error(ERR_STRING_MAX_LIMIT);
                return FailureE;
            }
        }
        if (len >= rd.lmts.containerlimit) {
            idbgx(Debug::ser_bin, "error");
            rd.err = make_error(ERR_CONTAINER_LIMIT);
            return FailureE;
        }

        if (len > V) {
            idbgx(Debug::ser_bin, "error");
            rd.err = make_error(ERR_BITSET_SIZE);
            return FailureE;
        }

        _rfd.f = &DeserializerBase::loadBitsetContinue<V>;
        _rfd.s = 0;
        return ContinueE;
    }

    template <size_t V>
    static ReturnValues loadBitsetContinue(Base& _rd, FncData& _rfd, void* /*_pctx*/)
    {
        DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

        if (!rd.cpb) {
            rd.estk.pop();
            return SuccessE;
        }

        std::bitset<V>* pbs    = reinterpret_cast<std::bitset<V>*>(_rfd.p);
        uint64_t&       len    = rd.estk.top().first_uint64_t_value();
        size_t          bitoff = 0;

        while ((rd.be - rd.cpb) > 0 && _rfd.s < len) {
            const uint8_t* puc = reinterpret_cast<const uint8_t*>(rd.cpb);

            pbs->set(_rfd.s, (*puc & (1 << bitoff)) != 0);

            ++_rfd.s;
            ++bitoff;

            if (bitoff == 8) {
                ++rd.cpb;
                bitoff = 0;
            }
        }

        if (_rfd.s < len) {
            return WaitE;
        } else if (bitoff) {
            ++rd.cpb;
        }

        rd.estk.pop();
        return SuccessE;
    }

    static ReturnValues loadBitvec(Base& _rd, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadBitvecBegin(Base& _rd, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadBitvecContinue(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    template <typename T>
    static ReturnValues load(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Des>
    static ReturnValues load(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Des, class Ctx>
    static ReturnValues load(Base& _rd, FncData& _rfd, void* /*_pctx*/);

    template <uint S>
    static ReturnValues loadBinary(Base& _rb, FncData& _rfd, void* /*_pctx*/);

    static ReturnValues loadBinaryString(Base& _rb, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadBinaryStringCheck(Base& _rb, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadUtf8(Base& _rb, FncData& _rfd, void* /*_pctx*/);

    template <typename T, class Des>
    static ReturnValues loadContainer(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        idbgx(Debug::ser_bin, "load generic non pointer container " << _rfd.n);
        DeserializerBase& rd = static_cast<DeserializerBase&>(_rb);
        if (!rd.cpb)
            return SuccessE;
        _rfd.f = &DeserializerBase::loadContainerBegin<T, Des>;
        _rfd.d = 0;
        rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64_t>, &_rfd.d));
        return ContinueE;
    }

    template <typename T, class Des>
    static ReturnValues loadContainerBegin(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rb));

        if (!rd.cpb) {
            rd.estk.pop();
            return SuccessE;
        }
        {
            const uint64_t i = _rfd.d;

            idbgx(Debug::ser_bin, " sz = " << i);

            if (i != InvalidIndex()) {
                uint64_t crcsz;
                if (check_value_with_crc(crcsz, i)) {
                    _rfd.d = crcsz;
                } else {
                    rd.err = make_error(ERR_CONTAINER_MAX_LIMIT);
                    return FailureE;
                }
            }
        }

        const uint64_t i(_rfd.d);

        vdbgx(Debug::ser_bin, "i = " << i);

        if (
            i != InvalidIndex() && i > rd.lmts.containerlimit) {
            idbgx(Debug::ser_bin, "error");
            rd.err = make_error(ERR_CONTAINER_LIMIT);
            return FailureE;
        }

        if (i == InvalidIndex()) {
            SOLID_ASSERT(!_rfd.s);
            T** c = reinterpret_cast<T**>(_rfd.p);
            *c    = nullptr;
            return SuccessE;
        } else if (!_rfd.s) {
            T** c = reinterpret_cast<T**>(_rfd.p);
            vdbgx(Debug::ser_bin, "");
            *c     = new T;
            _rfd.p = *c;
        }

        {
            T* c = reinterpret_cast<T*>(_rfd.p);
            container_reserve_helper(*c, i);
        }

        if (i) {
            FncData fd(_rfd); //we need a local copy
            rd.pushExtended((uint64_t)0);

            _rfd.f = &Base::popExtStack;
            _rfd.p = nullptr;

            fd.f = &DeserializerBase::loadContainerContinue<T, Des>;

            rd.fstk.push(fd);

            typename T::value_type* pvt = rd.estk.top().generic(typename T::value_type());
            rd.push(*pvt);

            return ContinueE;
        }
        return SuccessE;
    }

    template <typename T, class Des>
    static ReturnValues loadContainerContinue(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        Des&                    rd(static_cast<Des&>(_rb));
        uint64_t&               ri  = _rfd.d;
        T*                      c   = reinterpret_cast<T*>(_rfd.p);
        typename T::value_type* pvt = rd.estk.top().template genericCast<typename T::value_type>();

        --ri;
        c->insert(c->end(), std::move(*pvt));

        if (rd.cpb && ri) {
            rd.push(*pvt);
            return ContinueE;
        }
        return SuccessE;
    }

    template <class Des>
    static ReturnValues loadArrayInt8(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        idbgx(Debug::ser_bin, "load int8 array");

        DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

        if (!rd.cpb) {
            return SuccessE;
        }
        {
            const uint64_t& rsz(_rfd.s);
            uint64_t        crcsz;
            idbgx(Debug::ser_bin, "size " << rsz);

            if (check_value_with_crc(crcsz, rsz)) {
                _rfd.s = crcsz;
            } else {
                rd.err = make_error(ERR_ARRAY_MAX_LIMIT);
                return FailureE;
            }
        }

        const uint64_t& rsz(_rfd.s);
        size_t&         rreturn_sz(*reinterpret_cast<size_t*>(rd.estk.top().first_void_value())); //the returned size

        idbgx(Debug::ser_bin, "size " << rsz);

        if (rsz > rd.lmts.containerlimit || rsz > _rfd.d) {
            idbgx(Debug::ser_bin, "error");
            rd.err = make_error(ERR_ARRAY_LIMIT);
            return FailureE;
        }

        rreturn_sz = rsz;

        _rfd.f = &DeserializerBase::loadBinary<0>;
        return ContinueE;
    }

    template <typename T, class Des>
    static ReturnValues loadArray(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        idbgx(Debug::ser_bin, "load generic array");
        DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
        if (!rd.cpb) {
            return SuccessE;
        }
        {
            const uint64_t& rsz(_rfd.s);
            uint64_t        crcsz;
            idbgx(Debug::ser_bin, "size " << rsz);

            if (check_value_with_crc(crcsz, rsz)) {
                _rfd.s = crcsz;
            } else {
                rd.err = make_error(ERR_ARRAY_MAX_LIMIT);
                return FailureE;
            }
        }

        const uint64_t& rsz(_rfd.s);
        size_t&         rreturn_sz(*reinterpret_cast<size_t*>(rd.estk.top().first_void_value())); //the returned size

        idbgx(Debug::ser_bin, "size " << rsz);

        if (rsz > rd.lmts.containerlimit || rsz > _rfd.d) {
            idbgx(Debug::ser_bin, "error");
            rd.err = make_error(ERR_ARRAY_LIMIT);
            return FailureE;
        }

        rreturn_sz = rsz;

        _rfd.f = &DeserializerBase::loadArrayContinue<T, Des>;
        _rfd.d = 0; //index
        return ContinueE;
    }

    template <typename T, class Des>
    static ReturnValues loadArrayContinue(Base& _rb, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rd(static_cast<Des&>(_rb));

        if (!rd.cpb) {
            return SuccessE;
        }

        idbgx(Debug::ser_bin, "load generic array continue " << _rfd.n << " idx = " << _rfd.d << " sz = " << _rfd.s);

        if (rd.cpb && _rfd.d < _rfd.s) {
            T* c = reinterpret_cast<T*>(_rfd.p);
            rd.push(c[_rfd.d]);
            ++_rfd.d;
            return ContinueE;
        }
        return SuccessE;
    }

    //! Internal callback for parsing a stream
    static ReturnValues loadStream(Base& _rb, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadStreamBegin(Base& _rb, FncData& _rfd, void* /*_pctx*/);
    static ReturnValues loadStreamCheck(Base& _rb, FncData& _rfd, void* /*_pctx*/);
    //! Internal callback for parsign a dummy stream
    /*!
        This is internally called when a deserialization destionation
        ostream could not be optained, or there was an error while
        writing to destination stream.
    */
    static ReturnValues loadDummyStream(Base& _rb, FncData& _rfd, void* /*_pctx*/);

    template <class Des>
    static ReturnValues loadCall(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        Des& rs(static_cast<Des&>(_rs));
        //const uint32_t    val = _rfd.s;

        if (!rs.cpb) {
            rs.estk.pop();
            return SuccessE;
        }

        typename Des::CallT call = std::move(*rs.estk.top().template genericCast<typename Des::CallT>());

        rs.estk.pop();

        call(rs, _rfd.s, rs.err);

        if (rs.err) {
            _rfd.f = loadReturnError;
        } else {
            _rfd.f = loadReturnSuccess;
        }
        return ContinueE;
    }

    template <class Des, class Ctx>
    static ReturnValues loadCall(Base& _rs, FncData& _rfd, void* _pctx)
    {
        Des& rs(static_cast<Des&>(_rs));

        if (!rs.cpb) {
            return SuccessE;
        }

        Ctx& rctx = *reinterpret_cast<Ctx*>(_pctx);

        typename Des::CallT call = std::move(*rs.estk.top().template genericCast<typename Des::CallT>());

        rs.estk.pop();

        call(rs, rctx, _rfd.s, rs.err);

        if (rs.err) {
            _rfd.f = loadReturnError;
        } else {
            _rfd.f = loadReturnSuccess;
        }
        return ContinueE;
    }

    static ReturnValues loadReturnError(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        DeserializerBase& rs(static_cast<DeserializerBase&>(_rs));
        if (!rs.err) {
            rs.err = make_error(static_cast<Errors>(_rfd.s));
        }
        return FailureE;
    }

    static ReturnValues loadReturnSuccess(Base& _rs, FncData& _rfd, void* /*_pctx*/)
    {
        return SuccessE;
    }

    void doPushStringLimit();
    void doPushStringLimit(size_t _v);
    void doPushStreamLimit();
    void doPushStreamLimit(uint64_t _v);
    void doPushContainerLimit();
    void doPushContainerLimit(size_t _v);
    int  run(const char* _pb, size_t _bl, void* _pctx);

public:
    typedef void ContextT;

    enum { IsSerializer = false,
        IsDeserializer  = true };
    static const char* loadValue(const char* _ps, uint8_t& _val);
    static const char* loadValue(const char* _ps, uint16_t& _val);
    static const char* loadValue(const char* _ps, uint32_t& _val);
    static const char* loadValue(const char* _ps, uint64_t& _val);

    DeserializerBase()
        : pb(nullptr)
        , cpb(nullptr)
        , be(nullptr)
    {
        tmpstr.reserve(sizeof(uint32_t));
    }
    DeserializerBase(
        Limits const& _rdefaultlmts)
        : Base(_rdefaultlmts)
        , pb(nullptr)
        , cpb(nullptr)
        , be(nullptr)
    {
        tmpstr.reserve(sizeof(uint32_t));
    }
    ~DeserializerBase();

    void clear();

    bool empty() const { return fstk.empty(); }

private:
    template <class T>
    friend struct DeserializerPushHelper;
    friend class Base;
    const char* pb;
    const char* cpb;
    const char* be;
    std::string tmpstr;
};

template <>
ReturnValues DeserializerBase::loadBinary<0>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::loadBinary<1>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::loadBinary<2>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::loadBinary<4>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::loadBinary<8>(Base& _rb, FncData& _rfd, void* /*_pctx*/);

template <>
ReturnValues DeserializerBase::load<bool>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<int8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<uint8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<int16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<uint16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<int32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<uint32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);

template <>
ReturnValues DeserializerBase::load<int64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<uint64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/);
template <>
ReturnValues DeserializerBase::load<std::string>(Base& _rb, FncData& _rfd, void* /*_pctx*/);

template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint8_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/);
template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint16_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/);
template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint32_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/);
template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint64_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/);

template <typename T>
ReturnValues DeserializerBase::load(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    //should never get here
    return FailureE;
}
template <typename T, class Des>
ReturnValues DeserializerBase::load(Base& _rd, FncData& _rfd, void* _pctx)
{
    idbgx(Debug::ser_bin, "load generic non pointer");
    Des& rd(static_cast<Des&>(_rd));
    if (!rd.cpb)
        return SuccessE;
    rd.fstk.pop();
    solidSerialize(rd, *reinterpret_cast<T*>(_rfd.p));
    return ContinueE;
}

template <typename T, class Des, class Ctx>
ReturnValues DeserializerBase::load(Base& _rd, FncData& _rfd, void* _pctx)
{
    idbgx(Debug::ser_bin, "load generic non pointer");
    Des& rd(static_cast<Des&>(_rd));
    if (!rd.cpb)
        return SuccessE;
    rd.fstk.pop();
    Ctx& rctx = *reinterpret_cast<Ctx*>(_pctx);
    //*reinterpret_cast<T*>(_rfd.p) & rd;
    solidSerialize(rd, *reinterpret_cast<T*>(_rfd.p), rctx);
    return ContinueE;
}

template <>
struct DeserializerPushHelper<bool> {
    void operator()(DeserializerBase& _rs, bool& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<bool>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<int8_t> {
    void operator()(DeserializerBase& _rs, int8_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int8_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<uint8_t> {
    void operator()(DeserializerBase& _rs, uint8_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint8_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<uint16_t> {
    void operator()(DeserializerBase& _rs, uint16_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint16_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<int16_t> {
    void operator()(DeserializerBase& _rs, int16_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int16_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<uint32_t> {
    void operator()(DeserializerBase& _rs, uint32_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint32_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<int32_t> {
    void operator()(DeserializerBase& _rs, int32_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int32_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<uint64_t> {
    void operator()(DeserializerBase& _rs, uint64_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint64_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<int64_t> {
    void operator()(DeserializerBase& _rs, int64_t& _rv, const char* _name, bool _b = false)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int64_t>, &_rv, _name));
    }
};

template <>
struct DeserializerPushHelper<std::string> {
    void operator()(DeserializerBase& _rs, std::string& _rv, const char* _name, bool _b = false)
    {
        _rv.clear();
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<std::string>, &_rv, _name));
    }
};

template <class T>
struct DeserializerPushHelper {
    template <class Des>
    void operator()(Des& _rs, T& _rv, const char* _name)
    {
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<T, Des>, &_rv, _name));
    }
    template <class Des>
    void operator()(Des& _rs, T& _rv, const char* _name, bool _b)
    {
        typedef typename Des::ContextT ContextT;
        _rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<T, Des, ContextT>, &_rv, _name));
    }
};

//--------------------------------------------------------------
//--------------------------------------------------------------
template <class Ctx = void>
class Deserializer;

template <>
class Deserializer<void> : public DeserializerBase {
public:
    using ContextT      = void;
    using DeserializerT = Deserializer<ContextT>;
    using BaseT         = DeserializerBase;
    using TypeIdMapT    = TypeIdMapDes<DeserializerT>;
    using CallT         = std::function<void(DeserializerT&, uint64_t, ErrorConditionT&)>;

    Deserializer(
        const TypeIdMapT* _ptypeidmap = nullptr)
        : ptypeidmap(_ptypeidmap)
    {
    }
    Deserializer(
        Limits const&     _rdefaultlmts,
        const TypeIdMapT* _ptypeidmap = nullptr)
        : BaseT(_rdefaultlmts)
        , ptypeidmap(_ptypeidmap)
    {
    }

    int run(const char* _pb, size_t _bl)
    {
        return DeserializerBase::run(_pb, _bl, nullptr);
    }

    Deserializer& pushStringLimit()
    {
        this->doPushStringLimit();
        return *this;
    }
    Deserializer& pushStringLimit(size_t _v)
    {
        this->doPushStringLimit(_v);
        return *this;
    }
    Deserializer& pushStreamLimit()
    {
        this->doPushStreamLimit();
        return *this;
    }
    Deserializer& pushStreamLimit(uint64_t _v)
    {
        this->doPushStreamLimit(_v);
        return *this;
    }
    Deserializer& pushContainerLimit()
    {
        this->doPushContainerLimit();
        return *this;
    }
    Deserializer& pushContainerLimit(size_t _v)
    {
        this->doPushContainerLimit(_v);
        return *this;
    }

    template <size_t V>
    Deserializer& push(std::array<uint8_t, V>& _rarray, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBinary<0>, _rarray.data(), _name, V));
        return *this;
    }

    template <size_t V>
    Deserializer& push(std::bitset<V>& _rbitset, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBitset<V>, &_rbitset, _name, 0));
        return *this;
    }

    Deserializer& push(std::vector<bool>& _rbitvec, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBitvec, &_rbitvec, _name, 0));
        return *this;
    }

    template <typename T>
    Deserializer& push(T& _t, const char* _name = Base::default_name)
    {
        DeserializerPushHelper<T> ph;
        ph(*this, _t, _name);
        return *this;
    }

    template <typename T>
    Deserializer& push(T*& _t, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            pushExtended((uint64_t)0);
            this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
            this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadPlainPointerPrepare<T, DeserializerT>, reinterpret_cast<void*>(&_t), _name));
        } else {
            DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T>
    Deserializer& push(std::shared_ptr<T>& _rptr, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            pushExtended((uint64_t)0);
            this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
            this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadSharedPointerPrepare<T, DeserializerT>, reinterpret_cast<void*>(&_rptr), _name));
        } else {
            DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T1, typename T2>
    Deserializer& push(std::pair<T1, T2>& _rpair, const char* _name = Base::default_name)
    {
        push(_rpair.second, "second");
        push(_rpair.first, "first");
        return *this;
    }

    template <class F>
    DeserializerT& pushCall(F _f, uint64_t _val, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::template loadCall<DeserializerT>, nullptr, _name, _val));
        pushExtended((uint64_t)0);

        DeserializerBase::estk.top().generic(CallT(_f));
        return *this;
    }

    //! Schedules a std (style) container for deserialization
    template <typename T>
    Deserializer& pushContainer(T& _t, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name));
        return *this;
    }
    //! Schedules a std (style) container for deserialization
    template <typename T>
    Deserializer& pushContainer(T*& _t, const char* _name = Base::default_name)
    {
        SOLID_ASSERT(!_t); //the pointer must be null!!
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name, 0));
        return *this;
    }
    Deserializer& pushBinary(void* _p, size_t _sz, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadBinary<0>, _p, _name, _sz));
        return *this;
    }

    Deserializer& pushArray(uint8_t* _p, size_t& _rsz, const size_t _cp, const char* _name = Base::default_name)
    {
        this->pushExtended(static_cast<void*>(&_rsz));
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArrayInt8<DeserializerT>, (void*)_p, _name, 0, _cp));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().s));
        return *this;
    }

    Deserializer& pushArray(int8_t* _p, size_t& _rsz, const size_t _cp, const char* _name = Base::default_name)
    {
        this->pushExtended(static_cast<void*>(&_rsz));
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArrayInt8<DeserializerT>, (void*)_p, _name, 0, _cp));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().s));
        return *this;
    }

    template <typename T>
    Deserializer& pushArray(T* _p, size_t& _rsz, const size_t _cp, const char* _name = Base::default_name)
    {
        this->pushExtended(static_cast<void*>(&_rsz));
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)_p, _name, 0, _cp));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().s));
        return *this;
    }
#if 0
    //disabled for safety
    template <typename T>
    Deserializer& pushDynamicArray(T*& _p, size_t& _rsz, const char* _name = Base::default_name)
    {
        this->pushExtended((void*)&_rsz);
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)&_p, _name, InvalidSize(), 0));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().d));
        return *this;
    }
#endif
    Deserializer& pushUtf8(
        std::string& _str, const char* _name = Base::default_name)
    {
        _str.clear();
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadUtf8, &_str, _name, 0));
        return *this;
    }

    Deserializer& pushStream(
        std::ostream* _ps, const char* _name = Base::default_name)
    {
        if (_ps) {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, -1ULL));
        } else {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, -1ULL));
        }
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, 0));
        return *this;
    }
    Deserializer& pushStream(
        std::ostream*   _ps,
        const uint64_t& _rat,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name)
    {
        if (_ps) {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, _rlen));
        } else {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, _rlen));
        }
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, _rat));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamCheck, _ps, _name, _rlen));
        return *this;
    }
    Deserializer& pushStream(
        std::istream* _ps, const char* _name = Base::default_name);
    Deserializer& pushStream(
        std::istream*   _ps,
        const uint64_t& _rat,
        const uint64_t&,
        const char* _name = Base::default_name);
    Deserializer& pushCross(uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint8_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint16_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint16_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint32_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint32_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint64_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &_rv, _name));
        return *this;
    }

    DeserializerT& pushValue(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, Base::ERR_DESERIALIZE_VALUE));
        return *this;
    }
    const TypeIdMapT* typeIdMap() const
    {
        return ptypeidmap;
    }
    DeserializerT& pushNoop(const size_t _count, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(FncData(&Base::noop, nullptr, _name, _count));
        return *this;
    }

private:
    template <typename T>
    DeserializerT& push(const T& _t, const char* _name = Base::default_name)
    {
        return push(const_cast<T&>(_t), _name);
    }

    const TypeIdMapT* ptypeidmap;
};

//--------------------------------------------------------------
//--------------------------------------------------------------

template <class Ctx>
class Deserializer : public DeserializerBase {
public:
    using ContextT      = Ctx;
    using DeserializerT = Deserializer<ContextT>;
    using BaseT         = DeserializerBase;
    using TypeIdMapT    = TypeIdMapDes<DeserializerT>;
    using CallT         = std::function<void(DeserializerT&, ContextT&, uint64_t, ErrorConditionT&)>;

    Deserializer(
        const TypeIdMapT* _ptypeidmap = nullptr)
        : ptypeidmap(_ptypeidmap)
    {
    }
    Deserializer(
        Limits const&     _rdefaultlmts,
        const TypeIdMapT* _ptypeidmap = nullptr)
        : BaseT(_rdefaultlmts)
        , ptypeidmap(_ptypeidmap)
    {
    }

    int run(const char* _pb, size_t _bl, Ctx& _rctx)
    {
        const void* pctx = &_rctx;
        return DeserializerBase::run(_pb, _bl, const_cast<void*>(pctx));
    }

    Deserializer& pushStringLimit()
    {
        this->doPushStringLimit();
        return *this;
    }
    Deserializer& pushStringLimit(size_t _v)
    {
        this->doPushStringLimit(_v);
        return *this;
    }
    Deserializer& pushStreamLimit()
    {
        this->doPushStreamLimit();
        return *this;
    }
    Deserializer& pushStreamLimit(uint64_t _v)
    {
        this->doPushStreamLimit(_v);
        return *this;
    }
    Deserializer& pushContainerLimit()
    {
        this->doPushContainerLimit();
        return *this;
    }
    Deserializer& pushContainerLimit(size_t _v)
    {
        this->doPushContainerLimit(_v);
        return *this;
    }

    template <size_t V>
    Deserializer& push(std::array<uint8_t, V>& _rarray, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBinary<0>, _rarray.data(), _name, V));
        return *this;
    }

    template <size_t V>
    Deserializer& push(std::bitset<V>& _rbitset, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBitset<V>, &_rbitset, _name, 0));
        return *this;
    }

    Deserializer& push(std::vector<bool>& _rbitvec, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadBitvec, &_rbitvec, _name, 0));
        return *this;
    }

    template <typename T>
    Deserializer& push(T& _t, const char* _name = Base::default_name)
    {
        DeserializerPushHelper<T> ph;
        ph(*this, _t, _name, true);
        return *this;
    }

    template <typename T>
    Deserializer& push(T*& _t, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            this->pushExtended((uint64_t)0);
            this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
            this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadPlainPointerPrepare<T, DeserializerT>, reinterpret_cast<void*>(&_t), _name));
        } else {
            DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T>
    Deserializer& push(std::shared_ptr<T>& _rptr, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            this->pushExtended((uint64_t)0);
            this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
            this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadSharedPointerPrepare<T, DeserializerT>, reinterpret_cast<void*>(&_rptr), _name));
        } else {
            DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T>
    Deserializer& push(std::shared_ptr<T>& _rptr, const uint64_t _type_id, const char* _name = Base::default_name)
    {
        if (ptypeidmap) {
            void*       p = nullptr;
            std::string tmpstr;

            err = typeIdMap()->template load<T>(*this, &p, _type_id, tmpstr, _name);

            _rptr = reinterpret_cast<T*>(p);

            if (err) {
                DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_POINTER_UNKNOWN));
            }
        } else {
            DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, DeserializerBase::ERR_NO_TYPE_MAP));
        }
        return *this;
    }

    template <typename T1, typename T2>
    Deserializer& push(std::pair<T1, T2>& _rpair, const char* _name = Base::default_name)
    {
        push(_rpair.second, "second");
        push(_rpair.first, "first");
        return *this;
    }

    template <class F>
    DeserializerT& pushCall(F _f, uint64_t _val, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(SerializerBase::FncData(&DeserializerBase::template loadCall<DeserializerT, Ctx>, nullptr, _name, _val));
        this->pushExtended((uint64_t)0);

        DeserializerBase::estk.top().generic(CallT(_f));
        return *this;
    }
    //! Schedules a std (style) container for deserialization
    template <typename T>
    Deserializer& pushContainer(T& _t, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name));
        return *this;
    }
    //! Schedules a std (style) container for deserialization
    template <typename T>
    Deserializer& pushContainer(T*& _t, const char* _name = Base::default_name)
    {
        SOLID_ASSERT(!_t); //the pointer must be null!!
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name, 0));
        return *this;
    }
    Deserializer& pushBinary(void* _p, size_t _sz, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadBinary<0>, _p, _name, _sz));
        return *this;
    }

    template <typename T>
    Deserializer& pushArray(T* _p, size_t& _rsz, const size_t _cp, const char* _name = Base::default_name)
    {
        this->pushExtended(static_cast<void*>(&_rsz));
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)_p, _name, 0, _cp));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().s));
        return *this;
    }
#if 0
    //disabled for safety
    template <typename T>
    Deserializer& pushDynamicArray(T*& _p, size_t& _rsz, const char* _name = Base::default_name)
    {
        this->pushExtended((void*)&_rsz);
        this->Base::fstk.push(FncData(&Base::popExtStack, nullptr));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)&_p, _name, InvalidSize(), 0));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &fstk.top().d));
        return *this;
    }
#endif
    Deserializer& pushUtf8(
        std::string& _str, const char* _name = Base::default_name)
    {
        _str.clear();
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadUtf8, &_str, _name, 0));
        return *this;
    }

    Deserializer& pushStream(
        std::ostream* _ps, const char* _name = Base::default_name)
    {
        if (_ps) {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, -1ULL));
        } else {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, -1ULL));
        }
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, 0));
        return *this;
    }
    Deserializer& pushStream(
        std::ostream*   _ps,
        const uint64_t& _rat,
        const uint64_t& _rlen,
        const char*     _name = Base::default_name)
    {
        if (_ps) {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, _rlen));
        } else {
            this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, _rlen));
        }
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, _rat));
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamCheck, _ps, _name, _rlen));
        return *this;
    }
    Deserializer& pushStream(
        std::istream* _ps, const char* _name = Base::default_name);
    Deserializer& pushStream(
        std::istream*   _ps,
        const uint64_t& _rat,
        const uint64_t&,
        const char* _name = Base::default_name);
    Deserializer& pushCross(uint8_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint8_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint16_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint16_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint32_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint32_t>, &_rv, _name));
        return *this;
    }
    Deserializer& pushCross(uint64_t& _rv, const char* _name = Base::default_name)
    {
        this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64_t>, &_rv, _name));
        return *this;
    }

    DeserializerT& pushValue(const uint8_t& _rv, const char* _name = Base::default_name)
    {
        DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::loadReturnError, nullptr, _name, Base::ERR_DESERIALIZE_VALUE));
        return *this;
    }

    const TypeIdMapT* typeIdMap() const
    {
        return ptypeidmap;
    }

private:
    template <typename T>
    DeserializerT& push(const T& _t, const char* _name = Base::default_name)
    {
        return push(const_cast<T&>(_t), _name);
    }

    const TypeIdMapT* ptypeidmap;
};

} //namespace binary
} //namespace serialization
} //namespace solid
