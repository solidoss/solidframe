
#pragma once

#include <type_traits>
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

} //namespace solid
