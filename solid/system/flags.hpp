// solid/system/flags.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <bitset>
#include <initializer_list>
#include <type_traits>

namespace solid {

template <class E, E Last = E::LastFlag>
class Flags {
public:
    using UTypeT                  = typename std::underlying_type<E>::type;
    using ThisT                   = Flags<E, Last>;
    using FlagT                   = E;
    static constexpr size_t count = 1 + static_cast<UTypeT>(Last);

private:
    std::bitset<count> bits_;

public:
    constexpr bool isSet(const E _e) const
    {
        return bits_[static_cast<UTypeT>(_e)];
    }

    constexpr bool has(const E _e) const
    {
        return isSet(_e);
    }

    ThisT& set(const E _e)
    {
        bits_.set(static_cast<UTypeT>(_e));
        return *this;
    }

    ThisT& reset(const E _e)
    {
        bits_.reset(static_cast<UTypeT>(_e));
        return *this;
    }

    ThisT& flip(const E _e)
    {
        bits_.flip(static_cast<UTypeT>(_e));
        return *this;
    }

    Flags() {}

    Flags(unsigned long long _val)
        : bits_(_val)
    {
    }

    Flags(std::initializer_list<E> init_lst)
    {

        for (const E& it : init_lst) {
            set(it);
        }
    }

    ThisT& operator|=(const Flags& _flags)
    {
        bits_ |= _flags.bits_;
        return *this;
    }

    ThisT& operator&=(const Flags& _flags)
    {
        bits_ &= _flags.bits_;
        return *this;
    }

    ThisT& operator|=(const E _flag)
    {
        return set(_flag);
    }

    ThisT& operator&=(const E _flag)
    {
        const ThisT f{_flag};
        bits_ &= _flag.bits_;
        return *this;
    }

    ThisT& reset()
    {
        bits_.reset();
        return *this;
    }

    UTypeT toUnderlyingType() const
    {
        return static_cast<UTypeT>(bits_.to_ullong());
    }

    std::string toString(const char _zero = '0', const char _one = '1')
    {
        return bits_.to_string(_zero, _one);
    }
};

template <class E, E L>
Flags<E, L> operator|(const Flags<E, L>& _f1, const Flags<E, L>& _f2)
{
    Flags<E, L> f(_f1);
    f |= _f2;
    return f;
}

template <class E, E L>
Flags<E, L> operator&(const Flags<E, L>& _f1, const Flags<E, L>& _f2)
{
    Flags<E, L> f(_f1);
    f &= _f2;
    return f;
}

template <class E, E L>
Flags<E, L> operator|(const Flags<E, L>& _f1, const E _f2)
{
    Flags<E, L> f(_f1);
    f |= _f2;
    return f;
}

template <class E, E L>
Flags<E, L> operator&(const Flags<E, L>& _f1, const E _f2)
{
    Flags<E, L> f(_f1);
    f &= _f2;
    return f;
}

} // namespace solid
