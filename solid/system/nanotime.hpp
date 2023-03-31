// solid/system/nanotime.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "convertors.hpp"
#include <chrono>
#include <time.h>

namespace solid {

//! A timespec wrapper
/*!
    Basicaly it is a pair of seconds and nanoseconds.
*/
struct NanoTime : public timespec {
    using SecondT     = decltype(timespec::tv_sec);
    using NanoSecondT = decltype(timespec::tv_nsec);

    static NanoTime max()
    {
        return NanoTime{std::numeric_limits<SecondT>::max(), std::numeric_limits<NanoSecondT>::max()};
    }

    template <class Rep, class Period>
    NanoTime(const std::chrono::duration<Rep, Period>& _duration)
    {
        tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(_duration).count();
        tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(_duration).count() % 1000000000L;
    }

    template <class Clock, class Duration>
    NanoTime(const std::chrono::time_point<Clock, Duration>& _time_point)
    {
        const auto duration = _time_point.time_since_epoch();
        tv_sec              = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        tv_nsec             = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000L;
    }

    NanoTime(const SecondT _sec = 0, const NanoSecondT _nsec = 0)
    {
        tv_sec  = _sec;
        tv_nsec = _nsec;
    }

    static NanoTime nowSystem()
    {
        return NanoTime(std::chrono::system_clock::now());
    }

    static NanoTime nowSteady()
    {
        return NanoTime(std::chrono::steady_clock::now());
    }

    template <class TimePoint>
    TimePoint timePointCast() const
    {
        const typename TimePoint::duration dur = std::chrono::duration_cast<typename TimePoint::duration>(std::chrono::seconds(tv_sec) + std::chrono::nanoseconds(tv_nsec));
        return TimePoint() + dur;
    }

    template <class TimePoint, class MyClock>
    TimePoint timePointClockCast() const
    {
        using MyTimePoint = std::chrono::time_point<MyClock, typename TimePoint::duration>;

        const MyTimePoint my_time;
        TimePoint         re_time;

        doClockCast(re_time, my_time);
        return re_time;
    }

    template <class Duration>
    Duration durationCast() const
    {
        return std::chrono::duration_cast<Duration>(std::chrono::seconds(tv_sec) + std::chrono::nanoseconds(tv_nsec));
    }

    template <class Rep, class Period>
    NanoTime& operator=(const std::chrono::duration<Rep, Period>& _duration)
    {
        tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(_duration).count();
        tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(_duration).count() % 1000000000L;
        return *this;
    }

    template <class Clock, class Duration>
    NanoTime& operator=(const std::chrono::time_point<Clock, Duration>& _time_point)
    {
        const auto duration = _time_point.time_since_epoch();
        tv_sec              = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        tv_nsec             = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000L;
        return *this;
    }

    auto seconds() const { return tv_sec; }
    auto nanoSeconds() const { return tv_nsec; }

    bool isMax() const;

    bool operator!=(const NanoTime& _ts) const;
    bool operator==(const NanoTime& _ts) const;
    bool operator>=(const NanoTime& _ts) const;
    bool operator>(const NanoTime& _ts) const;
    bool operator<=(const NanoTime& _ts) const;
    bool operator<(const NanoTime& _ts) const;

private:
    template <class Clock, class Duration>
    void doClockCast(std::chrono::time_point<Clock, Duration>& _rtp, const std::chrono::time_point<Clock, Duration>& /*_rmytp*/) const
    {
        _rtp = timePointCast<std::chrono::time_point<Clock, Duration>>();
    }

    template <class Clock, class MyClock, class Duration>
    void doClockCast(std::chrono::time_point<Clock, Duration>& _rtp, const std::chrono::time_point<MyClock, Duration>& /*_rmytp*/) const
    {
        const typename Clock::time_point   other_now = Clock::now();
        const typename MyClock::time_point my_now    = MyClock::now();
        const typename MyClock::time_point my_tp     = timePointCast<typename MyClock::time_point>();
        const auto                         delta     = my_tp - my_now;
        _rtp                                         = std::chrono::time_point_cast<Duration>(other_now + delta);
    }
};
template <class Rep, class Period>
constexpr NanoTime operator+(NanoTime _first, const std::chrono::duration<Rep, Period>& _duration)
{
    _first.tv_sec += std::chrono::duration_cast<std::chrono::seconds>(_duration).count();
    _first.tv_nsec += std::chrono::duration_cast<std::chrono::nanoseconds>(_duration).count() % 1000000000L;

    _first.tv_sec += (_first.tv_nsec / 1000000000L);
    _first.tv_nsec = (_first.tv_nsec % 1000000000L);
    return _first;
}

namespace detail {
template <class Clock, class RetDuration, class Duration>
void time_point_clock_cast(std::chrono::time_point<Clock, RetDuration>& _rret_tp, const std::chrono::time_point<Clock, Duration>& _rtp)
{
    _rret_tp = std::chrono::time_point_cast<Duration>(_rtp);
}

template <class RetClock, class RetDuration, class Clock, class Duration>
void time_point_clock_cast(std::chrono::time_point<RetClock, RetDuration>& /*_rret_tp*/, const std::chrono::time_point<Clock, Duration>& _rtp)
{
    const typename RetClock::time_point ret_now = RetClock::now();
    const typename Clock::time_point    my_now  = Clock::now();
    const typename Clock::time_point    my_tp   = std::chrono::time_point_cast<typename Clock::duration>(_rtp);
    const auto                          delta   = my_tp - my_now;
    _rtp                                        = std::chrono::time_point_cast<typename RetClock::duration>(ret_now + delta);
}
} // namespace detail

template <class RetClock, class Clock, class Duration>
typename RetClock::time_point time_point_clock_cast(const std::chrono::time_point<Clock, Duration>& _rtp)
{
    typename RetClock::time_point ret_tp;

    detail::time_point_clock_cast(ret_tp, _rtp);
    return ret_tp;
}

std::ostream& operator<<(std::ostream& _ros, const NanoTime& _ntime);

#ifndef SOLID_HAS_NO_INLINES
#include "solid/system/nanotime.ipp"
#endif

} // namespace solid
