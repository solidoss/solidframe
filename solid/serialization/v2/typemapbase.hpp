// solid/serialization/v2/typemapbase.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/error.hpp"
#include "solid/utility/common.hpp"
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace solid {
namespace serialization {
namespace v2 {

struct Base {
    virtual ~Base();

private:
    friend class TypeMapBase;
    virtual void baseError(const ErrorConditionT& _err) = 0;
};

class TypeMapBase {
protected:
    using TypeIndexMapT    = std::unordered_map<std::type_index, size_t>;
    using PointerFunctionT = std::function<void(void*)>;

    TypeIndexMapT type_map_;

    static ErrorConditionT error_no_cast();
    static ErrorConditionT error_no_type();

public:
    virtual ~TypeMapBase();

    template <class S, typename... Args>
    S createSerializer(Args&&... _args) const
    {
        return S(*this, std::forward<Args>(_args)...);
    }

    template <class D, typename... Args>
    D createDeserializer(Args&&... _args) const
    {
        return D(*this, std::forward<Args>(_args)...);
    }

    template <class T>
    size_t index(const T* _p) const
    {
        const auto it = type_map_.find(std::type_index(typeid(*_p)));
        if (it != type_map_.cend()) {
            return it->second;
        } else {
            return InvalidIndex();
        }
    }

    template <typename TypeId, class T>
    size_t id(TypeId& _rtypeid, const T* _p, ErrorConditionT& _rerr) const
    {
        if (_p != nullptr) {
            const auto it = type_map_.find(std::type_index(typeid(*_p)));
            if (it != type_map_.cend()) {
                getTypeId(it->second, std::addressof(_rtypeid));
                return it->second;
            } else {
                _rerr = error_no_type();
                return InvalidIndex();
            }
        } else {
            return getNullTypeId(std::addressof(_rtypeid));
        }
    }

    template <class T>
    void serialize(Base& _rs, const T* _pt, const size_t _idx, const char* _name) const
    {
        const ErrorConditionT err = doSerialize(_rs, _pt, _idx, _name);
        if (err) {
            _rs.baseError(err);
        }
    }

    template <class T, class Ctx>
    void serialize(Base& _rs, const T* _pt, const size_t _idx, Ctx& _rctx, const char* _name) const
    {
        const ErrorConditionT err = doSerialize(_rs, _pt, _idx, std::addressof(_rctx), _name);
        if (err) {
            _rs.baseError(err);
        }
    }

    template <typename TypeId, class T>
    void deserialize(Base& _rd, std::shared_ptr<T>& _rsp, const TypeId& _rtypeid, const char* _name) const
    {
        const ErrorConditionT err = doDeserializeShared(_rd, std::addressof(_rsp), std::type_index(typeid(T)), std::addressof(_rtypeid), _name);
        if (err) {
            _rd.baseError(err);
        }
    }

    template <typename TypeId, class T, class Ctx>
    void deserialize(Base& _rd, std::shared_ptr<T>& _rsp, const TypeId& _rtypeid, Ctx& _rctx, const char* _name) const
    {
        const ErrorConditionT err = doDeserializeShared(_rd, std::addressof(_rsp), std::type_index(typeid(T)), std::addressof(_rtypeid), std::addressof(_rctx), _name);
        if (err) {
            _rd.baseError(err);
        }
    }

    template <typename TypeId, class T, class D>
    void deserialize(Base& _rd, std::unique_ptr<T, D>& _rup, const TypeId& _rtypeid, const char* _name) const
    {
        auto lambda = [&_rup](void* _pv) {
            _rup.reset(reinterpret_cast<T*>(_pv));
        };
        const ErrorConditionT err = doDeserializeUnique(_rd, std::cref(lambda), std::type_index(typeid(T)), std::addressof(_rtypeid), _name);

        if (err) {
            _rd.baseError(err);
        }
    }
    template <typename TypeId, class T, class D, class Ctx>
    void deserialize(Base& _rd, std::unique_ptr<T, D>& _rup, const TypeId& _rtypeid, Ctx& _rctx, const char* _name) const
    {
        auto lambda = [&_rup](void* _pv) {
            _rup.reset(reinterpret_cast<T*>(_pv));
        };
        const ErrorConditionT err = doDeserializeUnique(_rd, std::cref(lambda), std::type_index(typeid(T)), std::addressof(_rtypeid), std::addressof(_rctx), _name);

        if (err) {
            _rd.baseError(err);
        }
    }

private:
    virtual void   getTypeId(const size_t _idx, void* _ptype_id) const = 0;
    virtual size_t getNullTypeId(void* _ptype_id) const                = 0;

    virtual ErrorConditionT doSerialize(Base& _rs, const void* _pt, const size_t _idx, const char* _name) const              = 0;
    virtual ErrorConditionT doSerialize(Base& _rs, const void* _pt, const size_t _idx, void* _pctx, const char* _name) const = 0;

    virtual ErrorConditionT doDeserializeUnique(Base& _rd, const PointerFunctionT& _fnc, const std::type_index& _spt, const void* _ptype_id, const char* _name) const              = 0;
    virtual ErrorConditionT doDeserializeUnique(Base& _rd, const PointerFunctionT& _fnc, const std::type_index& _spt, const void* _ptype_id, void* _pctx, const char* _name) const = 0;

    virtual ErrorConditionT doDeserializeShared(Base& _rd, void* _psp, const std::type_index& _spt, const void* _ptype_id, const char* _name) const              = 0;
    virtual ErrorConditionT doDeserializeShared(Base& _rd, void* _psp, const std::type_index& _spt, const void* _ptype_id, void* _pctx, const char* _name) const = 0;
};

} //namespace v2
} //namespace serialization
} // namespace solid
