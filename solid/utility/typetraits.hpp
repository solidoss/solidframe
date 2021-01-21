
#pragma once

#include <type_traits>
#include <bitset>
#include <memory>
#include <utility>

namespace solid {

// template <class T1, class T2>
// T1 make_choice_help(T1 _t1, T2 _t2, std::true_type _tt){
//     return _t1;
// }
//
// template <class T1, class T2>
// T2 make_choice_help(T1 _t1, T2 _t2, std::false_type _tt){
//     return _t2;
// }
//
//
// template <bool B, typename T1, typename T2>
// typename std::conditional<B, T1, T2>::type make_choice(T1 _t1, T2 _t2){
//     return make_choice_help(_t1, _t2, std::integral_constant<bool, B>());
// }

// ----------------------------------------------------------------------------

template <class T1, class T2>
T1 if_then_else_help(T1&& _t1, T2&& _t2, std::true_type _tt)
{
    return T1(std::move(_t1));
}

template <class T1, class T2>
T2 if_then_else_help(T1&& _t1, T2&& _t2, std::false_type _tt)
{
    return _t2;
}

template <bool B, typename T1, typename T2>
typename std::conditional<B, T1, T2>::type if_then_else(T1&& _then, T2&& _else)
{
    return if_then_else_help(std::move(_then), std::move(_else), std::integral_constant<bool, B>());
}

//https://stackoverflow.com/questions/12042824/how-to-write-a-type-trait-is-container-or-is-vector
template <typename T, typename _ = void>
struct is_container : std::false_type {
};

template <typename... Ts>
struct is_container_helper {
};

template <typename T>
struct is_container<
    T,
    typename std::conditional<
        false,
        is_container_helper<
            typename T::value_type,
            typename T::size_type,
            typename T::allocator_type,
            typename T::iterator,
            typename T::const_iterator,
            decltype(std::declval<T>().size()),
            decltype(std::declval<T>().begin()),
            decltype(std::declval<T>().end()),
            decltype(std::declval<T>().cbegin()),
            decltype(std::declval<T>().cend())>,
        void>::type> : public std::true_type {
};

template <class F, class... Args>
struct is_callable_helper {
    template <class U>
    static auto test(U* p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
    template <class U>
    static auto test(...) -> decltype(std::false_type());

    static constexpr bool value = decltype(test<F>(0))::value;
    using type                  = decltype(test<F>(0));
};

template <class F, class... Args>
struct is_callable : std::conditional<is_callable_helper<F, Args...>::value, std::true_type, std::false_type>::type {
};

template <class _Type, template <class...> class _Template>
inline constexpr bool is_specialization_v = false;
template <template <class...> class _Template, class... _Types>
inline constexpr bool is_specialization_v<_Template<_Types...>, _Template> = true;

template <class _Type, template <class...> class _Template>
struct is_specialization : std::bool_constant<is_specialization_v<_Type, _Template>> {
};


template <typename T>
struct is_unique_ptr: std::false_type{};

template <typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type{};

template <class T>
struct is_shared_ptr: std::false_type{};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type{};

template< class T >
inline constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

template< class T >
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <typename T>
struct is_bitset: std::false_type{};

template <size_t Sz>
struct is_bitset<std::bitset<Sz>>: std::true_type{};

template< class T >
inline constexpr bool is_bitset_v = is_bitset<T>::value;

} //namespace solid
