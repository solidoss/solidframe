// solid/reflection/v2/typemap.hpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "solid/reflection/v2/reflection_concept.hpp"
#include "solid/system/common.hpp"
#include "solid/system/exception.hpp"

namespace solid::reflection::v2 {

// —— TypeMap ——
// Registry of the types that may travel behind a polymorphic smart pointer,
// each one known by a (category, id) pair and a name. Reflectors (e.g. the
// binary v4 serializer/deserializer) use it to:
//  - write the dynamic type of a pointed object and walk it as that type
//    (TypeMapBase::reflect), and
//  - create the object named by a (category, id) pair, install it in a
//    shared_ptr/unique_ptr to one of its registered bases and walk it
//    (TypeMapBase::create_and_reflect).
//
// Compared to reflection::v1::TypeMap:
//  - per type stubs are plain function pointers instantiated at registration,
//    no std::function and no global reflector registry: a reflector's slot is
//    found from the address of a per-type tag variable;
//  - walking an existing object needs no cast lookup: the most-derived object
//    is reached via dynamic_cast<void const*>;
//  - (category, id) lookups use a dense vector for small ids;
//  - the dynamic type lookup tries the std::type_info address before falling
//    back to the (string hashing) std::type_index map.
//
// A reflector R used with TypeMap<R...> must provide:
//  - typename R::ContextT;
//  - static constexpr bool R::is_const_reflector;
//  - template <class T> void R::reflect_object(T [const]& _rt, ContextT& _rctx).

namespace detail {
template <class R>
inline constexpr char reflector_tag = 0;

// The smart pointers a TypeMap can create objects into (see is_pointer_v).
enum struct PointerKindE : uint8_t {
    Shared = 0,
    Unique,
    Intrusive,
    MutableIntrusive,
    ConstIntrusive,
};

inline constexpr size_t pointer_kind_count = 5;

template <class Ptr>
consteval PointerKindE pointer_kind()
{
    if constexpr (is_shared_ptr_v<Ptr>) {
        return PointerKindE::Shared;
    } else if constexpr (is_unique_ptr_v<Ptr>) {
        static_assert(std::is_same_v<Ptr, std::unique_ptr<typename Ptr::element_type>>, "only std::unique_ptr with the default deleter is supported");
        return PointerKindE::Unique;
    } else if constexpr (solid::is_intrusive_ptr_v<Ptr>) {
        return PointerKindE::Intrusive;
    } else if constexpr (solid::is_mutable_intrusive_ptr_v<Ptr>) {
        return PointerKindE::MutableIntrusive;
    } else {
        static_assert(solid::is_const_intrusive_ptr_v<Ptr>, "unsupported pointer type");
        return PointerKindE::ConstIntrusive;
    }
}

template <PointerKindE Kind, class T>
struct pointer_type;
template <class T>
struct pointer_type<PointerKindE::Shared, T> {
    using type = std::shared_ptr<T>;
};
template <class T>
struct pointer_type<PointerKindE::Unique, T> {
    using type = std::unique_ptr<T>;
};
template <class T>
struct pointer_type<PointerKindE::Intrusive, T> {
    using type = solid::IntrusivePtr<T>;
};
template <class T>
struct pointer_type<PointerKindE::MutableIntrusive, T> {
    using type = solid::MutableIntrusivePtr<T>;
};
template <class T>
struct pointer_type<PointerKindE::ConstIntrusive, T> {
    using type = solid::ConstIntrusivePtr<T>;
};

template <PointerKindE Kind, class T>
using pointer_t = typename pointer_type<Kind, T>::type;

// Intrusive pointers exist only for types with an intrusive policy.
template <PointerKindE Kind, class T>
consteval bool pointer_kind_supports()
{
    if constexpr (Kind == PointerKindE::Shared || Kind == PointerKindE::Unique) {
        return true;
    } else {
        return requires { typename solid::intrusive_policy_dispatch_t<T>; };
    }
}
} // namespace detail

// Strong types for the (category, id, name) a type is registered with, so the
// arguments cannot be swapped by mistake:
//     _rmap.template register_type<Circle, Shape>(TypeCategory{1}, TypeId{4}, TypeName{"Circle"});
// The category may be omitted (it defaults to 0):
//     _rmap.template register_type<Circle, Shape>(TypeId{4}, TypeName{"Circle"});
// Only a (category, id) pair TypeId of the binary serializer carries the
// category on the wire; with an integral one, every type is in category 0.
struct TypeCategory {
    size_t value = 0;

    constexpr TypeCategory() noexcept = default;
    constexpr explicit TypeCategory(const size_t _value) noexcept
        : value(_value)
    {
    }
    constexpr bool operator==(TypeCategory const&) const noexcept = default;
};

struct TypeId {
    size_t value = 0;

    constexpr TypeId() noexcept = default;
    constexpr explicit TypeId(const size_t _value) noexcept
        : value(_value)
    {
    }
    constexpr bool operator==(TypeId const&) const noexcept = default;
};

struct TypeName {
    std::string_view value;

    constexpr TypeName() noexcept = default;
    constexpr explicit TypeName(const std::string_view _value) noexcept
        : value(_value)
    {
    }
    constexpr bool operator==(TypeName const&) const noexcept = default;
};

class TypeMapBase : NonCopyable {
public:
    struct TypeEntry {
        size_t                index_    = 0;
        size_t                category_ = 0;
        size_t                id_       = 0;
        std::string           name_;
        std::type_info const* ptype_info_ = &typeid(void);

        size_t index() const noexcept
        {
            return index_;
        }
        size_t category() const noexcept
        {
            return category_;
        }
        size_t id() const noexcept
        {
            return id_;
        }
        std::string_view name() const noexcept
        {
            return name_;
        }
        std::type_info const& type_info() const noexcept
        {
            return *ptype_info_;
        }
    };

    virtual ~TypeMapBase() = default;

    // Entry for the type whose std::type_info is _rti, nullptr if unknown.
    TypeEntry const* find(std::type_info const& _rti) const noexcept
    {
        if (const auto it = type_ptr_map_.find(&_rti); it != type_ptr_map_.end()) {
            return &types_[it->second];
        }
        if (const auto it = type_index_map_.find(std::type_index(_rti)); it != type_index_map_.end()) {
            return &types_[it->second];
        }
        return nullptr;
    }

    TypeEntry const* find(const TypeCategory _category, const TypeId _id) const noexcept
    {
        if (_category.value < category_vec_.size()) {
            const auto& rcategory = category_vec_[_category.value];
            if (_id.value < rcategory.dense_ids_.size()) {
                const auto index = rcategory.dense_ids_[_id.value];
                return index != 0 ? &types_[index] : nullptr;
            }
            if (const auto it = rcategory.sparse_ids_.find(_id.value); it != rcategory.sparse_ids_.end()) {
                return &types_[it->second];
            }
        }
        return nullptr;
    }

    TypeEntry const* find(const TypeCategory _category, const TypeName _name) const noexcept
    {
        if (_category.value < category_vec_.size()) {
            const auto& rcategory = category_vec_[_category.value];
            if (const auto it = rcategory.names_.find(_name.value); it != rcategory.names_.end()) {
                return &types_[it->second];
            }
        }
        return nullptr;
    }

    // Entry for the dynamic type of *_pvalue, nullptr if _pvalue is null or
    // its type is not registered.
    template <class T>
    TypeEntry const* find_dynamic(T const* _pvalue) const noexcept
    {
        return _pvalue != nullptr ? find(typeid(*_pvalue)) : nullptr;
    }

    // Registration index of the dynamic type of *_pvalue: 0 for nullptr,
    // throws for unregistered types.
    template <class T>
    size_t index(T const* _pvalue) const
    {
        if (_pvalue == nullptr) {
            return 0;
        }
        const auto* pentry = find(typeid(*_pvalue));
        solid_check(pentry != nullptr, "Unknown type: " << typeid(*_pvalue).name());
        return pentry->index_;
    }

    size_t size() const noexcept
    {
        return types_.size() - 1;
    }

    // Walks *_pvalue (typed as a base of the registered type _rentry) as the
    // registered type, through _rreflector.reflect_object.
    template <class Reflector, class T>
    void reflect(Reflector& _rreflector, T& _rvalue, typename Reflector::ContextT& _rctx, TypeEntry const& _rentry) const
    {
        const auto& rstub = static_cast<TypeStub const&>(_rentry);
        void*       pobj  = const_cast<void*>(most_derived(std::addressof(_rvalue)));
        rstub.reflector_vec_[reflectorSlot<Reflector>()].reflect_fnc_(&_rreflector, pobj, &_rctx);
    }

    // Creates the object registered as _rentry, stores it into _rptr (a
    // shared_ptr, unique_ptr or solid intrusive pointer to the registered type
    // or one of its registered bases) and walks it through _rreflector.reflect_object.
    // Returns false (leaving _rptr empty) if the pointer's element type is not
    // a registered base of the entry or the pointer kind cannot be created.
    template <class Reflector, class Ptr>
    bool create_and_reflect(Reflector& _rreflector, Ptr& _rptr, typename Reflector::ContextT& _rctx, TypeEntry const& _rentry) const
    {
        using ElementT     = typename Ptr::element_type;
        const auto& rstub  = static_cast<TypeStub const&>(_rentry);
        const auto* pcast  = rstub.findCast(typeid(ElementT));
        const auto& rrstub = rstub.reflector_vec_[reflectorSlot<Reflector>()];
        void*       pobj   = nullptr;

        constexpr auto kind = static_cast<size_t>(detail::pointer_kind<Ptr>());

        _rptr.reset();
        if (pcast == nullptr || rrstub.create_fnc_[kind] == nullptr || pcast->cast_fnc_[kind] == nullptr) {
            return false;
        }
        pobj = rrstub.create_fnc_[kind](rstub.pcreate_.get(), &_rctx, &_rptr, pcast->cast_fnc_[kind]);
        if (pobj == nullptr) {
            return false;
        }
        rrstub.reflect_fnc_(&_rreflector, pobj, &_rctx);
        return true;
    }

protected:
    using ReflectFunctionT = void (*)(void* /*_preflector*/, void* /*_pobject*/, void* /*_pctx*/);
    using CastFunctionT    = void (*)(void* /*_pto*/, void* /*_pfrom*/);
    using CreateFunctionT  = void* (*)(void const* /*_pcreate*/, void* /*_pctx*/, void* /*_pptr*/, CastFunctionT);

    struct ReflectorStub {
        ReflectFunctionT                                        reflect_fnc_ = nullptr;
        std::array<CreateFunctionT, detail::pointer_kind_count> create_fnc_{}; // by PointerKindE
    };

    struct CastStub {
        std::type_info const*                                 ptype_info_ = nullptr;
        std::array<CastFunctionT, detail::pointer_kind_count> cast_fnc_{}; // by PointerKindE
    };

    struct TypeStub : TypeEntry {
        std::vector<ReflectorStub> reflector_vec_;
        std::vector<CastStub>      cast_vec_;
        std::shared_ptr<void>      pcreate_; // user create function, if any

        CastStub const* findCast(std::type_info const& _rti) const noexcept
        {
            for (const auto& rcast : cast_vec_) {
                if (rcast.ptype_info_ == &_rti) {
                    return &rcast;
                }
            }
            for (const auto& rcast : cast_vec_) {
                if (*rcast.ptype_info_ == _rti) {
                    return &rcast;
                }
            }
            return nullptr;
        }
    };

    struct Category {
        static constexpr size_t dense_limit = 4096;

        std::vector<size_t>                          dense_ids_; // id -> index, 0 = none
        std::unordered_map<size_t, size_t>           sparse_ids_;
        std::unordered_map<std::string_view, size_t> names_;
    };

    TypeMapBase()
    {
        types_.emplace_back(); // index 0 is reserved for nullptr
    }

    template <class T>
    static void const* most_derived(T const* _pvalue) noexcept
    {
        if constexpr (std::is_polymorphic_v<T>) {
            return dynamic_cast<void const*>(_pvalue);
        } else {
            return _pvalue;
        }
    }

    template <class Reflector>
    size_t reflectorSlot() const
    {
        const void* ptag = &detail::reflector_tag<Reflector>;
        for (size_t i = 0; i < reflector_tag_vec_.size(); ++i) {
            if (reflector_tag_vec_[i] == ptag) {
                return i;
            }
        }
        solid_throw("Reflector not registered on TypeMap: " << typeid(Reflector).name());
        return 0;
    }

    TypeStub& doRegisterType(std::type_info const& _rti, const TypeCategory _type_category, const TypeId _type_id, const TypeName _type_name)
    {
        const size_t           _category = _type_category.value;
        const size_t           _id       = _type_id.value;
        const std::string_view _name     = _type_name.value;

        solid_check(find(_rti) == nullptr, "Type " << _rti.name() << " already registered");
        solid_check(find(_type_category, _type_id) == nullptr, "Id " << _id << " already exists in category " << _category);
        solid_check(find(_type_category, _type_name) == nullptr, "Name " << _name << " already exists in category " << _category);
        solid_check(_id != 0, "Id 0 is reserved for nullptr");

        const size_t index = types_.size();
        auto&        rstub = types_.emplace_back();
        rstub.index_       = index;
        rstub.category_    = _category;
        rstub.id_          = _id;
        rstub.name_        = _name;
        rstub.ptype_info_  = &_rti;
        rstub.reflector_vec_.resize(reflector_tag_vec_.size());

        type_ptr_map_[&_rti]                   = index;
        type_index_map_[std::type_index(_rti)] = index;

        if (category_vec_.size() <= _category) {
            category_vec_.resize(_category + 1);
        }
        auto& rcategory = category_vec_[_category];
        if (_id < Category::dense_limit) {
            if (rcategory.dense_ids_.size() <= _id) {
                rcategory.dense_ids_.resize(_id + 1, 0);
            }
            rcategory.dense_ids_[_id] = index;
        } else {
            rcategory.sparse_ids_[_id] = index;
        }
        rcategory.names_[rstub.name_] = index; // rstub.name_ is stable: types_ is a deque
        return rstub;
    }

    std::vector<void const*> reflector_tag_vec_;

private:
    std::deque<TypeStub>                              types_;
    std::unordered_map<std::type_info const*, size_t> type_ptr_map_;
    std::unordered_map<std::type_index, size_t>       type_index_map_;
    std::vector<Category>                             category_vec_;
};

template <class... Reflector>
class TypeMap : public TypeMapBase {
    using ThisT = TypeMap<Reflector...>;

    template <detail::PointerKindE Kind, class T, class B>
    static void castStub(void* _pto, void* _pfrom)
    {
        *static_cast<detail::pointer_t<Kind, B>*>(_pto) = std::move(*static_cast<detail::pointer_t<Kind, T>*>(_pfrom));
    }

    template <detail::PointerKindE Kind, class T, class B>
    static constexpr CastFunctionT castFunction()
    {
        if constexpr (detail::pointer_kind_supports<Kind, T>() && detail::pointer_kind_supports<Kind, B>()) {
            return &castStub<Kind, T, B>;
        } else {
            return nullptr;
        }
    }

    template <class T, class B, size_t... K>
    static constexpr std::array<CastFunctionT, detail::pointer_kind_count> castFunctions(std::index_sequence<K...>)
    {
        return {castFunction<static_cast<detail::PointerKindE>(K), T, B>()...};
    }

    template <class R, class T>
    static void reflectStub(void* _preflector, void* _pobject, void* _pctx)
    {
        auto& rreflector = *static_cast<R*>(_preflector);
        auto& rctx       = *static_cast<typename R::ContextT*>(_pctx);
        if constexpr (R::is_const_reflector) {
            rreflector.reflect_object(*static_cast<T const*>(_pobject), rctx);
        } else {
            rreflector.reflect_object(*static_cast<T*>(_pobject), rctx);
        }
    }

    template <class R, class T, class CreateF, detail::PointerKindE Kind>
    static void* createStub(void const* _pcreate, void* _pctx, void* _pptr, CastFunctionT _cast_fnc)
    {
        auto&                      rctx = *static_cast<typename R::ContextT*>(_pctx);
        detail::pointer_t<Kind, T> ptr;
        if constexpr (std::is_void_v<CreateF>) {
            detail::make_pointer<T>(ptr);
        } else {
            (*static_cast<CreateF const*>(_pcreate))(rctx, ptr);
        }
        void* const pobject = const_cast<void*>(static_cast<void const*>(ptr.get()));
        _cast_fnc(_pptr, &ptr);
        return pobject;
    }

    template <class R, class T, class CreateF, detail::PointerKindE Kind>
    static constexpr CreateFunctionT createFunction()
    {
        if constexpr (!detail::pointer_kind_supports<Kind, T>()) {
            return nullptr;
        } else if constexpr (std::is_void_v<CreateF>) {
            if constexpr (std::is_default_constructible_v<T> && !std::is_abstract_v<T>) {
                return &createStub<R, T, CreateF, Kind>;
            } else {
                return nullptr;
            }
        } else if constexpr (std::is_invocable_v<CreateF const&, typename R::ContextT&, detail::pointer_t<Kind, T>&>) {
            return &createStub<R, T, CreateF, Kind>;
        } else {
            return nullptr;
        }
    }

    template <class R, class T, class CreateF, size_t... K>
    static constexpr std::array<CreateFunctionT, detail::pointer_kind_count> createFunctions(std::index_sequence<K...>)
    {
        return {createFunction<R, T, CreateF, static_cast<detail::PointerKindE>(K)>()...};
    }

    template <class T, class CreateF>
    void fillStubs(TypeStub& _rstub)
    {
        size_t slot = 0;
        ((_rstub.reflector_vec_[slot++] = ReflectorStub{
              &reflectStub<Reflector, T>,
              createFunctions<Reflector, T, CreateF>(std::make_index_sequence<detail::pointer_kind_count>{})}),
            ...);
    }

    template <class T, class B>
    void addCast(TypeStub& _rstub)
    {
        static_assert(std::is_same_v<T, B> || std::is_base_of_v<B, T>, "B must be T or a base of T");
        for (const auto& rcast : _rstub.cast_vec_) {
            if (*rcast.ptype_info_ == typeid(B)) {
                return;
            }
        }
        _rstub.cast_vec_.push_back(CastStub{&typeid(B), castFunctions<T, B>(std::make_index_sequence<detail::pointer_kind_count>{})});
    }

    template <class T, class CreateF, class... Bases>
    size_t registerType(const TypeCategory _category, const TypeId _id, const TypeName _name, std::shared_ptr<void> _pcreate)
    {
        auto& rstub    = this->doRegisterType(typeid(T), _category, _id, _name);
        rstub.pcreate_ = std::move(_pcreate);
        fillStubs<T, CreateF>(rstub);
        addCast<T, T>(rstub);
        (addCast<T, Bases>(rstub), ...);
        return rstub.index_;
    }

public:
    class Proxy {
        ThisT& rmap_;

    public:
        explicit Proxy(ThisT& _rmap)
            : rmap_(_rmap)
        {
        }

        // T reachable through pointers to T only.
        template <class T>
        size_t register_type(const TypeCategory _category, const TypeId _id, const TypeName _name)
        {
            return rmap_.template registerType<T, void>(_category, _id, _name, nullptr);
        }

        template <class T>
        size_t register_type(const TypeId _id, const TypeName _name)
        {
            return register_type<T>(TypeCategory{}, _id, _name);
        }

        // T reachable through pointers to T or to its base B.
        template <class T, class B>
        size_t register_type(const TypeCategory _category, const TypeId _id, const TypeName _name)
        {
            return rmap_.template registerType<T, void, B>(_category, _id, _name, nullptr);
        }

        template <class T, class B>
        size_t register_type(const TypeId _id, const TypeName _name)
        {
            return register_type<T, B>(TypeCategory{}, _id, _name);
        }

        // As above with a custom create function, invoked as
        // _create_fnc(ContextT&, std::shared_ptr<T>&) or
        // _create_fnc(ContextT&, std::unique_ptr<T>&).
        template <class T, class CreateF>
        size_t register_type(const TypeCategory _category, const TypeId _id, const TypeName _name, CreateF _create_fnc)
        {
            using CreateT = std::decay_t<CreateF>;
            return rmap_.template registerType<T, CreateT>(_category, _id, _name, std::make_shared<CreateT>(std::move(_create_fnc)));
        }

        template <class T, class CreateF>
        size_t register_type(const TypeId _id, const TypeName _name, CreateF _create_fnc)
        {
            return register_type<T>(TypeCategory{}, _id, _name, std::move(_create_fnc));
        }

        template <class T, class B, class CreateF>
        size_t register_type(const TypeCategory _category, const TypeId _id, const TypeName _name, CreateF _create_fnc)
        {
            using CreateT = std::decay_t<CreateF>;
            return rmap_.template registerType<T, CreateT, B>(_category, _id, _name, std::make_shared<CreateT>(std::move(_create_fnc)));
        }

        template <class T, class B, class CreateF>
        size_t register_type(const TypeId _id, const TypeName _name, CreateF _create_fnc)
        {
            return register_type<T, B>(TypeCategory{}, _id, _name, std::move(_create_fnc));
        }

        // Makes the already registered T reachable through pointers to B too.
        template <class T, class B>
        void register_cast()
        {
            const auto* pentry = rmap_.find(typeid(T));
            solid_check(pentry != nullptr, "Type " << typeid(T).name() << " not registered");
            rmap_.template addCast<T, B>(const_cast<TypeStub&>(static_cast<TypeStub const&>(*pentry)));
        }
    };

    template <class InitF>
    explicit TypeMap(InitF&& _init_fnc)
    {
        (this->reflector_tag_vec_.push_back(&detail::reflector_tag<Reflector>), ...);
        Proxy proxy{*this};
        std::forward<InitF>(_init_fnc)(proxy);
    }
};

} // namespace solid::reflection::v2
