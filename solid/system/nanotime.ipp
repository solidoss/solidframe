// system/nanotime.ipp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef SOLID_HAS_NO_INLINES
#define inline
#endif

inline bool NanoTime::operator>=(const NanoTime& _ts) const
{
    return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec >= _ts.tv_nsec));
}

inline bool NanoTime::operator>(const NanoTime& _ts) const
{
    return (seconds() > _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec > _ts.tv_nsec));
}

inline bool NanoTime::operator<=(const NanoTime& _ts) const
{
    return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec <= _ts.tv_nsec));
}

inline bool NanoTime::operator<(const NanoTime& _ts) const
{
    return (seconds() < _ts.seconds()) || ((seconds() == _ts.seconds()) && (tv_nsec < _ts.tv_nsec));
}

inline bool NanoTime::operator!=(const NanoTime& _ts) const
{
    return tv_sec != _ts.tv_sec || tv_nsec != _ts.tv_nsec;
}

inline bool NanoTime::operator==(const NanoTime& _ts) const
{
    return tv_sec == _ts.tv_sec && tv_nsec == _ts.tv_nsec;
}

inline bool NanoTime::isMax() const
{
    return tv_sec == static_cast<decltype(tv_sec)>(-1);
}

inline std::ostream& operator<<(std::ostream& _ros, const NanoTime& _ntime)
{
    _ros << _ntime.seconds() << "s." << _ntime.nanoSeconds() << "ns";
    return _ros;
}

#ifdef SOLID_HAS_NO_INLINES
#undef inline
#endif
