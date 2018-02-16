// solid/serialization/typeidmap.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/function.hpp"
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace solid {
namespace serialization {

template <class T>
T* basic_factory()
{
    return new T;
}

bool joinTypeId(uint64_t& _rtype_id, const uint32_t _protocol_id, const uint64_t _message_id);

void splitTypeId(const uint64_t _type_id, uint32_t& _protocol_id, uint64_t& _message_id);

class TypeIdMapBase {
protected:
    //typedef void(*LoadFunctionT)(void*, void*, const char*);
    //typedef void(*StoreFunctionT)(void*, void*, const char*);
    typedef SOLID_FUNCTION(void(void*, void*, const char*)) LoadFunctionT;
    typedef SOLID_FUNCTION(void(void*, void*, const char*)) StoreFunctionT;

    typedef void (*CastFunctionT)(void*, void*);

    struct CastStub {
        CastStub(
            CastFunctionT _plain_cast  = nullptr,
            CastFunctionT _shared_cast = nullptr)
            : plain_cast(_plain_cast)
            , shared_cast(_shared_cast)
        {
        }

        CastFunctionT plain_cast;
        CastFunctionT shared_cast;
    };

    typedef SOLID_FUNCTION(void*(const CastFunctionT&, void*)) FactoryFunctionT;

    typedef std::pair<std::type_index, size_t> CastIdT;

    struct CastHash {
        size_t operator()(CastIdT const& _id) const
        {
            return _id.first.hash_code() ^ _id.second;
        }
    };

    struct ProtocolStub {
        ProtocolStub()
            : current_message_index(0)
        {
        }

        size_t current_message_index;
    };

    typedef std::unordered_map<CastIdT, CastStub, CastHash> CastMapT;

    struct Stub {
        FactoryFunctionT plain_factory;
        FactoryFunctionT shared_factory;
        LoadFunctionT    loadfnc;
        StoreFunctionT   storefnc;
        uint64_t         id;
    };

    static ErrorConditionT error_no_cast();
    static ErrorConditionT error_no_type();

    typedef std::vector<Stub> StubVectorT;

    typedef std::unordered_map<std::type_index, size_t> TypeIndexMapT;
    typedef std::unordered_map<uint64_t, size_t>        MessageTypeIdMapT;
    typedef std::unordered_map<size_t, ProtocolStub>    ProtocolMapT;

    template <class F>
    struct FactoryStub {
        F f;
        FactoryStub(F _f)
            : f(_f)
        {
        }
        void* operator()()
        {
            return nullptr;
        }
    };

    template <class T, class Ser>
    static void store_pointer(void* _pser, void* _pt, const char* _name)
    {
        Ser& rs = *(reinterpret_cast<Ser*>(_pser));
        T&   rt = *(reinterpret_cast<T*>(_pt));

        rs.push(rt, _name);
    }

    static void store_nullptr(void* _pser, void* _pt, const char* _name)
    {
    }

    template <class T, class Des>
    static void load_pointer(void* _pser, void* _pt, const char* _name)
    {
        Des& rs = *(reinterpret_cast<Des*>(_pser));
        T&   rt = *(reinterpret_cast<T*>(_pt));

        rs.push(rt, _name);
    }

    static void load_nullptr(void* _pser, void* _pt, const char* _name)
    {
    }

    template <class F, class T, class Ser>
    struct StoreFunctor {
        F f;
        StoreFunctor(F _f)
            : f(_f)
        {
        }

        void operator()(void* _pser, void* _pt, const char* _name)
        {
            Ser& rs = *(reinterpret_cast<Ser*>(_pser));
            T&   rt = *(reinterpret_cast<T*>(_pt));

            f(rs, rt, _name);
        }
    };

    template <class F, class T, class Des>
    struct LoadFunctor {
        F f;
        LoadFunctor(F _f)
            : f(_f)
        {
        }

        void operator()(void* _pser, void* _pt, const char* _name)
        {
            Des& rs = *(reinterpret_cast<Des*>(_pser));
            T&   rt = *(reinterpret_cast<T*>(_pt));

            f(rs, rt, _name);
        }
    };

    template <class Base, class Derived>
    static void cast_plain_pointer(void* _pderived, void* _pbase)
    {
        Derived* pd  = reinterpret_cast<Derived*>(_pderived);
        Base*&   rpb = *reinterpret_cast<Base**>(_pbase);
        rpb          = static_cast<Base*>(pd);
    }

    template <class Base, class Derived>
    static void cast_shared_pointer(void* _pderived, void* _pbase)
    {
        using DerivedSharedPtrT = std::shared_ptr<Derived>;
        using BaseSharedPtrT    = std::shared_ptr<Base>;

        DerivedSharedPtrT* pspder = reinterpret_cast<DerivedSharedPtrT*>(_pderived);
        BaseSharedPtrT*    pspbas = reinterpret_cast<BaseSharedPtrT*>(_pbase);

        if (pspder) {
            *pspbas = std::move(std::static_pointer_cast<Base>(std::move(*pspder)));
        } else {
            pspbas->reset();
        }
    }

    template <class T>
    static void* plain_factory(const CastFunctionT& _cast_fnc, void* _dest_ptr)
    {
        //TODO: try ... catch
        T* ptr = new T;
        _cast_fnc(ptr, _dest_ptr);
        return ptr;
    }

    template <class T>
    static void* shared_factory(const CastFunctionT& _cast_fnc, void* _dest_ptr)
    {
        //TODO: try ... catch
        std::shared_ptr<T> sp  = std::make_shared<T>();
        void*              ptr = sp.get();
        _cast_fnc(&sp, _dest_ptr);
        return ptr;
    }

    template <class T, class Allocator>
    struct SharedFactoryAlloc {
        Allocator a;
        SharedFactoryAlloc(Allocator _a)
            : a(_a)
        {
        }
        void* operator()(const CastFunctionT& _cast_fnc, void* _dest_ptr)
        {
            std::shared_ptr<T> sp  = std::allocate_shared<T>(a);
            void*              ptr = sp.get();
            _cast_fnc(&sp, _dest_ptr);
            return ptr;
        }
    };

    template <class T>
    static void cast_void_pointer(void* _pderived, void* _pbase)
    {
        T*& rpb = *reinterpret_cast<T**>(_pbase);
        rpb     = reinterpret_cast<T*>(_pderived);
    }

protected:
    TypeIdMapBase()
    {
        //register nullptr
        auto factory = [](const CastFunctionT& _cast_fnc, void* _dest_ptr) {
            _cast_fnc(nullptr, _dest_ptr);
            return nullptr;
        };
        stubvec.push_back(Stub());
        stubvec.back().plain_factory  = factory;
        stubvec.back().shared_factory = factory;
        stubvec.back().loadfnc        = &load_nullptr;
        stubvec.back().storefnc       = &store_nullptr;
        stubvec.back().id             = 0;

        msgidmap[0] = 0;
    }

    bool findTypeIndex(const uint64_t& _rid, size_t& _rindex) const;

    size_t doAllocateNewIndex(const size_t _protocol_id, uint64_t& _rid);
    bool   doFindTypeIndex(const size_t _protocol_id, size_t _idx, uint64_t& _rid) const;

    template <class T, class StoreF, class LoadF>
    size_t doRegisterType(StoreF _sf, LoadF _lf, const size_t _protocol_id, size_t _idx)
    {
        uint64_t id;

        if (_idx == 0) {
            _idx = doAllocateNewIndex(_protocol_id, id);
        } else if (doFindTypeIndex(_protocol_id, _idx, id)) {
            return InvalidIndex();
        }

        size_t vecidx = stubvec.size();

        typemap[std::type_index(typeid(T))] = vecidx;
        msgidmap[id]                        = vecidx;

        stubvec.push_back(Stub());
        stubvec.back().plain_factory  = &plain_factory<T>;
        stubvec.back().shared_factory = &shared_factory<T>;
        stubvec.back().loadfnc        = _lf;
        stubvec.back().storefnc       = _sf;
        stubvec.back().id             = id;

        doRegisterCast<T, T>();
        doRegisterCast<T>();

        return vecidx;
    }

    template <class T, class StoreF, class LoadF, class Allocator>
    size_t doRegisterTypeAlloc(StoreF _sf, LoadF _lf, Allocator _allocator, const size_t _protocol_id, size_t _idx)
    {
        uint64_t id;

        if (_idx == 0) {
            _idx = doAllocateNewIndex(_protocol_id, id);
        } else if (doFindTypeIndex(_protocol_id, _idx, id)) {
            return InvalidIndex();
        }

        size_t vecidx = stubvec.size();

        typemap[std::type_index(typeid(T))] = vecidx;
        msgidmap[id]                        = vecidx;

        stubvec.push_back(Stub());
        stubvec.back().plain_factory  = plain_factory<T>; //TODO: use a PlainFactoryAlloc
        stubvec.back().shared_factory = SharedFactoryAlloc<T, Allocator>(_allocator);
        stubvec.back().loadfnc        = _lf;
        stubvec.back().storefnc       = _sf;
        stubvec.back().id             = id;

        doRegisterCast<T, T>();
        doRegisterCast<T>();

        return vecidx;
    }

    template <class Derived, class Base>
    bool doRegisterDownCast()
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Base)));
        if (it != TypeIdMapBase::typemap.end()) {
            return doRegisterDownCast<Derived>(it->second);
        } else {
            SOLID_THROW("Base type not registered");
            return false;
        }
    }

    template <class Derived>
    bool doRegisterDownCast(const size_t _idx)
    {
        if (_idx < stubvec.size()) {
            typemap[std::type_index(typeid(Derived))] = _idx;
            return true;
        } else {
            SOLID_THROW("Invalid type index " << _idx);
            return false;
        }
    }

    template <class Derived, class Base>
    bool doRegisterCast()
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Derived)));
        if (it != TypeIdMapBase::typemap.end()) {
            castmap[CastIdT(std::type_index(typeid(Base)), it->second)] = CastStub(cast_plain_pointer<Base, Derived>, cast_shared_pointer<Base, Derived>);
            castmap[CastIdT(std::type_index(typeid(Derived)), 0)]       = &cast_void_pointer<Derived>;
            castmap[CastIdT(std::type_index(typeid(Base)), 0)]          = &cast_void_pointer<Base>;
            return true;
        } else {
            SOLID_THROW("Derived type not registered");
            return false;
        }
    }

    template <class Derived>
    bool doRegisterCast()
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Derived)));
        if (it != TypeIdMapBase::typemap.end()) {
            castmap[CastIdT(std::type_index(typeid(Derived)), 0)] = &cast_void_pointer<Derived>;
            return true;
        } else {
            SOLID_THROW("Derived type not registered");
            return false;
        }
    }

protected:
    TypeIndexMapT     typemap;
    CastMapT          castmap;
    StubVectorT       stubvec;
    MessageTypeIdMapT msgidmap;
    ProtocolMapT      protomap;
};

template <class Ser>
class TypeIdMapSer : virtual protected TypeIdMapBase {
public:
    TypeIdMapSer() {}

    template <class T>
    ErrorConditionT store(Ser& _rs, T* _pt, const char* _name) const
    {
        if (_pt == nullptr) {
            return storeNullPointer(_rs, _name);
        } else {
            return storePointer(_rs, _pt, std::type_index(typeid(*_pt)), _name);
        }
    }

    template <class T>
    ErrorConditionT store(Ser& _rs, T* _pt, const size_t _type_id, const char* _name) const
    {
        if (_pt == nullptr) {
            return storeNullPointer(_rs, _name);
        } else {
            return storePointer(_rs, _pt, _type_id, _name);
        }
    }

private:
    virtual ErrorConditionT storeNullPointer(Ser& _rs, const char* _name) const                               = 0;
    virtual ErrorConditionT storePointer(Ser& _rs, void* _p, std::type_index const&, const char* _name) const = 0;
    virtual ErrorConditionT storePointer(Ser& _rs, void* _p, const size_t _type_id, const char* _name) const  = 0;

    TypeIdMapSer(TypeIdMapSer&&);
    TypeIdMapSer& operator=(TypeIdMapSer&&);
};

template <class Des>
class TypeIdMapDes : virtual public TypeIdMapBase {
public:
    TypeIdMapDes() {}

    template <class T>
    ErrorConditionT loadPlainPointer(
        Des&               _rd,
        void*              _rptr, //store destination pointer, the real type must be static_cast-ed to this pointer
        const uint64_t&    _riv, //integer value that may store the typeid
        std::string const& _rsv, //string value that may store the typeid
        const char*        _name) const
    {
        return doLoadPlainPointer(_rd, _rptr, std::type_index(typeid(T)), _riv, _rsv, _name);
    }

    template <class T>
    ErrorConditionT loadSharedPointer(
        Des&               _rd,
        void*              _rptr, //store destination pointer, the real type must be static_cast-ed to this pointer
        const uint64_t&    _riv, //integer value that may store the typeid
        std::string const& _rsv, //string value that may store the typeid
        const char*        _name) const
    {
        return doLoadSharedPointer(_rd, _rptr, std::type_index(typeid(T)), _riv, _rsv, _name);
    }

    ErrorConditionT findTypeIndex(const uint64_t _type_id, size_t& _rstub_index) const
    {
        if (!TypeIdMapBase::findTypeIndex(_type_id, _rstub_index)) {
            return TypeIdMapBase::error_no_cast();
        }
        return ErrorConditionT();
    }

    virtual void loadTypeId(Des& _rd, uint64_t& _rv, std::string& _rstr, const char* _name) const = 0;

private:
    virtual ErrorConditionT doLoadPlainPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& _rsv, const char* _name) const = 0;

    virtual ErrorConditionT doLoadSharedPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& _rsv, const char* _name) const = 0;

    TypeIdMapDes(TypeIdMapDes&&);
    TypeIdMapDes& operator=(TypeIdMapDes&&);
};

template <class Ser, class Des, class Data = void>
class TypeIdMap;

template <class Ser, class Des>
class TypeIdMap<Ser, Des, void> : public TypeIdMapSer<Ser>, public TypeIdMapDes<Des> {
public:
    TypeIdMap()
    {
    }

    ~TypeIdMap()
    {
    }

    template <class T>
    size_t registerType(const size_t _protocol_id, const size_t _idx = 0)
    {
        return TypeIdMapBase::doRegisterType<T>(
            TypeIdMapBase::store_pointer<T, Ser>, TypeIdMapBase::load_pointer<T, Des>, _protocol_id, _idx);
    }

    template <class T, class Allocator>
    size_t registerTypeAlloc(Allocator _allocator, const size_t _protocol_id, const size_t _idx = 0)
    {
        return TypeIdMapBase::doRegisterTypeAlloc<T>(
            TypeIdMapBase::store_pointer<T, Ser>, TypeIdMapBase::load_pointer<T, Des>,
            _allocator, _protocol_id, _idx);
    }

    template <class T, class StoreF, class LoadF>
    size_t registerType(StoreF _sf, LoadF _lf, const size_t _protocol_id, const size_t _idx = 0)
    {
        TypeIdMapBase::StoreFunctor<StoreF, T, Ser> sf(_sf);
        TypeIdMapBase::LoadFunctor<LoadF, T, Des>   lf(_lf);
        return TypeIdMapBase::doRegisterType<T>(sf, lf, _protocol_id, _idx);
    }

    template <class T, class StoreF, class LoadF, class Allocator>
    size_t registerTypeAlloc(StoreF _sf, LoadF _lf, Allocator _allocator, const size_t _protocol_id, const size_t _idx = 0)
    {
        TypeIdMapBase::StoreFunctor<StoreF, T, Ser> sf(_sf);
        TypeIdMapBase::LoadFunctor<LoadF, T, Des>   lf(_lf);
        return TypeIdMapBase::doRegisterTypeAlloc<T>(sf, lf, _allocator, _protocol_id, _idx);
    }

    template <class Derived, class Base>
    bool registerDownCast()
    {
        return TypeIdMapBase::doRegisterDownCast<Derived, Base>();
    }

    template <class Derived, class Base>
    bool registerCast()
    {
        return TypeIdMapBase::doRegisterCast<Derived, Base>();
    }

private:
    /*virtual*/ ErrorConditionT storeNullPointer(Ser& _rs, const char* _name) const
    {
        static const uint32_t nulltypeid = 0;
        _rs.pushCross(nulltypeid, _name);
        return ErrorConditionT();
    }

    /*virtual*/ ErrorConditionT storePointer(Ser& _rs, void* _p, std::type_index const& _tid, const char* _name) const
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(_tid);
        if (it != TypeIdMapBase::typemap.end()) {
            TypeIdMapBase::Stub const& rstub = TypeIdMapBase::stubvec[it->second];
            rstub.storefnc(&_rs, _p, _name);
            _rs.pushCross(rstub.id, _name);
            return ErrorConditionT();
        }
        return TypeIdMapBase::error_no_type();
    }

    /*virtual*/ ErrorConditionT storePointer(Ser& _rs, void* _p, const size_t _type_id, const char* _name) const
    {
        if (_type_id < TypeIdMapBase::stubvec.size()) {
            TypeIdMapBase::Stub const& rstub = TypeIdMapBase::stubvec[_type_id];
            rstub.storefnc(&_rs, _p, _name);
            _rs.pushCross(rstub.id, _name);
            return ErrorConditionT();
        } else {
            return TypeIdMapBase::error_no_type();
        }
    }

    /*virtual*/ void loadTypeId(Des& _rd, uint64_t& _rv, std::string& /*_rstr*/, const char* _name) const
    {
        _rd.pushCross(_rv, _name);
    }

    // Returns no error, if the type identified by _riv exists
    // and a cast from that type to the type identified by _rtidx, exists
    /*virtual*/ ErrorConditionT doLoadPlainPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& /*_rsv*/, const char* _name) const
    {

        size_t stubindex;

        if (!TypeIdMapBase::findTypeIndex(_riv, stubindex)) {
            return TypeIdMapBase::error_no_cast();
        }

        TypeIdMapBase::CastMapT::const_iterator it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));

        if (it != TypeIdMapBase::castmap.end()) {
            TypeIdMapBase::Stub const& rstub   = TypeIdMapBase::stubvec[stubindex];
            void*                      realptr = rstub.plain_factory(it->second.plain_cast, _rptr);

            rstub.loadfnc(&_rd, realptr, _name);
            return ErrorConditionT();
        }
        return TypeIdMapBase::error_no_cast();
    }

    // Returns no error, if the type identified by _riv exists
    // and a cast from that type to the type identified by _rtidx, exists
    /*virtual*/ ErrorConditionT doLoadSharedPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& /*_rsv*/, const char* _name) const
    {

        size_t stubindex;

        if (!TypeIdMapBase::findTypeIndex(_riv, stubindex)) {
            return TypeIdMapBase::error_no_cast();
        }

        TypeIdMapBase::CastMapT::const_iterator it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));

        if (it != TypeIdMapBase::castmap.end()) {
            TypeIdMapBase::Stub const& rstub   = TypeIdMapBase::stubvec[stubindex];
            void*                      realptr = rstub.shared_factory(it->second.shared_cast, _rptr);

            rstub.loadfnc(&_rd, realptr, _name);
            return ErrorConditionT();
        }
        return TypeIdMapBase::error_no_cast();
    }

    void clear()
    {
        TypeIdMapBase::typemap.clear();
        TypeIdMapBase::castmap.clear();
        TypeIdMapBase::stubvec.clear();
        TypeIdMapBase::msgidmap.clear();
        TypeIdMapBase::protomap.clear();
    }

private:
    TypeIdMap(TypeIdMap const&);
    TypeIdMap(TypeIdMap&&);
    TypeIdMap& operator=(TypeIdMap const&);
    TypeIdMap& operator=(TypeIdMap&&);

private:
};

template <class Ser, class Des, class Data>
class TypeIdMap : public TypeIdMapSer<Ser>, public TypeIdMapDes<Des> {
    typedef std::vector<Data> DataVectorT;

public:
    TypeIdMap()
    {
        datavec.resize(1);
    }

    ~TypeIdMap()
    {
    }

    template <class T>
    size_t registerType(Data const& _rd, const size_t _protocol_id = 0, const size_t _idx = 0)
    {

        const size_t rv = TypeIdMapBase::doRegisterType<T>(
            TypeIdMapBase::store_pointer<T, Ser>,
            TypeIdMapBase::load_pointer<T, Des>,
            _protocol_id, _idx);

        if (rv == InvalidIndex()) {
            return rv;
        }

        datavec.push_back(_rd);
        return rv;
    }

    template <class T, class Allocator>
    size_t registerTypeAlloc(Data const& _rd, Allocator _allocator, const size_t _protocol_id = 0, const size_t _idx = 0)
    {

        const size_t rv = TypeIdMapBase::doRegisterTypeAlloc<T>(
            TypeIdMapBase::store_pointer<T, Ser>,
            TypeIdMapBase::load_pointer<T, Des>,
            _allocator, _protocol_id, _idx);

        if (rv == InvalidIndex()) {
            return rv;
        }

        datavec.push_back(_rd);
        return rv;
    }

    template <class T, class StoreF, class LoadF>
    size_t registerType(Data const& _rd, StoreF _sf, LoadF _lf, const size_t _protocol_id = 0, const size_t _idx = 0)
    {
        TypeIdMapBase::StoreFunctor<StoreF, T, Ser> sf(_sf);
        TypeIdMapBase::LoadFunctor<LoadF, T, Des>   lf(_lf);

        const size_t rv = TypeIdMapBase::doRegisterType<T>(sf, lf, _protocol_id, _idx);

        if (rv == InvalidIndex()) {
            return rv;
        }

        datavec.push_back(_rd);
        return rv;
    }

    template <class T, class StoreF, class LoadF, class Allocator>
    size_t registerTypeAlloc(Data const& _rd, StoreF _sf, LoadF _lf, Allocator _allocator, const size_t _protocol_id = 0, const size_t _idx = 0)
    {
        TypeIdMapBase::StoreFunctor<StoreF, T, Ser> sf(_sf);
        TypeIdMapBase::LoadFunctor<LoadF, T, Des>   lf(_lf);

        const size_t rv = TypeIdMapBase::doRegisterTypeAlloc<T>(sf, lf, _allocator, _protocol_id, _idx);

        if (rv == InvalidIndex()) {
            return rv;
        }

        datavec.push_back(_rd);
        return rv;
    }

    template <class Derived, class Base>
    bool registerDownCast()
    {
        return TypeIdMapBase::doRegisterDownCast<Derived, Base>();
    }

    template <class Derived, class Base>
    bool registerCast()
    {
        return TypeIdMapBase::doRegisterCast<Derived, Base>();
    }

    template <class T>
    Data& operator[](const T* _pt)
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
        if (it != TypeIdMapBase::typemap.end()) {
            return datavec[it->second];
        } else {
            return datavec[0];
        }
    }
    template <class T>
    Data const& operator[](const T* _pt) const
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
        if (it != TypeIdMapBase::typemap.end()) {
            return datavec[it->second];
        } else {
            return datavec[0];
        }
    }

    template <class T>
    size_t index(const T* _pt) const
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
        if (it != TypeIdMapBase::typemap.end()) {
            return it->second;
        } else {
            return 0;
        }
    }

    Data& operator[](const size_t _idx)
    {
        return datavec[_idx];
    }

    Data const& operator[](const size_t _idx) const
    {
        return datavec[_idx];
    }

private:
    /*virtual*/ ErrorConditionT storeNullPointer(Ser& _rs, const char* _name) const
    {
        static const uint32_t nulltypeid = 0;
        _rs.pushCross(nulltypeid, _name);
        return ErrorConditionT();
    }

    /*virtual*/ ErrorConditionT storePointer(Ser& _rs, void* _p, std::type_index const& _tid, const char* _name) const
    {
        TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(_tid);
        if (it != TypeIdMapBase::typemap.end()) {
            TypeIdMapBase::Stub const& rstub = TypeIdMapBase::stubvec[it->second];
            rstub.storefnc(&_rs, _p, _name);
            _rs.pushCross(rstub.id, _name);
            return ErrorConditionT();
        } else {
            return TypeIdMapBase::error_no_type();
        }
    }

    /*virtual*/ ErrorConditionT storePointer(Ser& _rs, void* _p, const size_t _type_id, const char* _name) const
    {
        if (_type_id < TypeIdMapBase::stubvec.size()) {
            TypeIdMapBase::Stub const& rstub = TypeIdMapBase::stubvec[_type_id];
            rstub.storefnc(&_rs, _p, _name);
            _rs.pushCross(rstub.id, _name);
            return ErrorConditionT();
        } else {
            return TypeIdMapBase::error_no_type();
        }
    }

    /*virtual*/ void loadTypeId(Des& _rd, uint64_t& _rv, std::string& /*_rstr*/, const char* _name) const
    {
        _rd.pushCross(_rv, _name);
    }

    // Returns no error, if the type identified by _riv exists
    // and a cast from that type to the type identified by _rtidx, exists
    /*virtual*/ ErrorConditionT doLoadPlainPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& /*_rsv*/, const char* _name) const
    {
        size_t stubindex;

        if (!TypeIdMapBase::findTypeIndex(_riv, stubindex)) {
            return TypeIdMapBase::error_no_cast();
        }

        TypeIdMapBase::CastMapT::const_iterator it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));

        if (it != TypeIdMapBase::castmap.end()) {
            TypeIdMapBase::Stub const& rstub   = TypeIdMapBase::stubvec[stubindex];
            void*                      realptr = rstub.plain_factory(it->second.plain_cast, _rptr);

            rstub.loadfnc(&_rd, realptr, _name);
            return ErrorConditionT();
        }
        return TypeIdMapBase::error_no_cast();
    }

    // Returns no error, if the type identified by _riv exists
    // and a cast from that type to the type identified by _rtidx, exists
    /*virtual*/ ErrorConditionT doLoadSharedPointer(
        Des& _rd, void* _rptr,
        std::type_index const& _rtidx, //type_index of the destination pointer
        const uint64_t& _riv, std::string const& /*_rsv*/, const char* _name) const
    {
        size_t stubindex;

        if (!TypeIdMapBase::findTypeIndex(_riv, stubindex)) {
            return TypeIdMapBase::error_no_cast();
        }

        TypeIdMapBase::CastMapT::const_iterator it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));

        if (it != TypeIdMapBase::castmap.end()) {
            TypeIdMapBase::Stub const& rstub   = TypeIdMapBase::stubvec[stubindex];
            void*                      realptr = rstub.shared_factory(it->second.shared_cast, _rptr);

            rstub.loadfnc(&_rd, realptr, _name);
            return ErrorConditionT();
        }
        return TypeIdMapBase::error_no_cast();
    }

    void clear()
    {
        TypeIdMapBase::typemap.clear();
        TypeIdMapBase::castmap.clear();
        TypeIdMapBase::stubvec.clear();
        TypeIdMapBase::msgidmap.clear();
        TypeIdMapBase::protomap.clear();

        datavec.clear();
    }

private:
    TypeIdMap(TypeIdMap const&);
    TypeIdMap(TypeIdMap&&);
    TypeIdMap& operator=(TypeIdMap const&);
    TypeIdMap& operator=(TypeIdMap&&);

private:
    DataVectorT datavec;
};

} //namespace serialization
} //namespace solid
