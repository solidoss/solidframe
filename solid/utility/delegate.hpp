// solid/event.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

namespace solid {

template <typename R, typename... A>
class Delegate;

template <typename R, typename... A>
class Delegate<R(A...)> {
    typedef R (*HelperFunctionT)(const void*, A...);

    template <typename T>
    static R helper(const void* _pfinal_fnc, A... _a)
    {
        return (*static_cast<const T*>(_pfinal_fnc))(std::forward<A...>(_a...));
    }
    const HelperFunctionT phelper_fnc_;
    const void*           pfinal_fnc_;

public:
    template <typename T>
    Delegate(T& _rt)
        : phelper_fnc_(&helper<T>)
        , pfinal_fnc_(&_rt)
    {
    }

    inline R operator()(A... _a)
    {
        return (*phelper_fnc_)(pfinal_fnc_, std::forward<A...>(_a...));
    }
    inline R operator()(A... _a) const
    {
        return (*phelper_fnc_)(pfinal_fnc_, std::forward<A...>(_a...));
    }
};
} // namespace solid
