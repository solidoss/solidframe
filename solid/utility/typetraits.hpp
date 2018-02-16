
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
T1 make_choice_help(T1&& _t1, T2&& _t2, std::true_type _tt)
{
    return T1(std::move(_t1));
}

template <class T1, class T2>
T2 make_choice_help(T1&& _t1, T2&& _t2, std::false_type _tt)
{
    return _t2;
}

template <bool B, typename T1, typename T2>
typename std::conditional<B, T1, T2>::type make_choice(T1&& _t1, T2&& _t2)
{
    return make_choice_help(std::move(_t1), std::move(_t2), std::integral_constant<bool, B>());
}

} //namespace solid
