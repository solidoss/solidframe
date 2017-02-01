#include <time.h>
#include "solid/system/convertors.hpp"
#include "solid/system/exception.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using namespace solid;
using namespace std;

struct NanoTime: public timespec{
    typedef UnsignedConvertor<time_t>::UnsignedType TimeT;
    //static const NanoTime maximum;
    
    NanoTime(){}
    
    template <class Rep, class Period>
    NanoTime(const std::chrono::duration<Rep, Period>& _duration){
        tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(_duration).count();
        tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(_duration).count() % 1000000000l;
    }
    
    template <class Clock, class Duration>
    NanoTime(const std::chrono::time_point<Clock, Duration>& _time_point){
        const auto duration = _time_point.time_since_epoch();
        tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000l;
    }
    
    static NanoTime createSystem(){
        return NanoTime(std::chrono::system_clock::now());
    }
    static NanoTime createSteady(){
        return NanoTime(std::chrono::steady_clock::now());
    }
    
    template <class TimePoint>
    TimePoint timePointCast()const{
        const typename TimePoint::duration dur = std::chrono::duration_cast<typename TimePoint::duration>(std::chrono::seconds(tv_sec) + std::chrono::nanoseconds(tv_nsec));
        return TimePoint() + dur;
    }
    
    template <class TimePoint, class MyClock>
    TimePoint timePointClockCast()const{
        //const typename TimePoint::duration d = std::chrono::duration_cast<typename TimePoint::duration>(std::chrono::seconds(tv_sec) + std::chrono::nanoseconds(tv_nsec));
        //return TimePoint() + d;
        
        using MyTimePoint = std::chrono::time_point<MyClock, typename TimePoint::duration>;
        
        const  MyTimePoint  my_time;
        TimePoint           re_time;
        //return std::chrono::time_point_cast<TimePoint>(my_time);
        doClockCast(re_time, my_time);
        return re_time;
    }
    
    template <class Duration>
    Duration durationCast()const{
        return std::chrono::duration_cast<Duration>(std::chrono::seconds(tv_sec) + std::chrono::nanoseconds(tv_nsec));
    }
    
    TimeT   seconds()const  {return tv_sec;}
    long nanoSeconds()const {return tv_nsec;}
    
    //bool isMax()const;
    
//     bool operator !=(const NanoTime &_ts)const;
//     bool operator ==(const NanoTime &_ts)const;
//     bool operator >=(const NanoTime &_ts)const;
//     bool operator >(const NanoTime &_ts)const;
//     bool operator <=(const NanoTime &_ts)const;
//     bool operator <(const NanoTime &_ts)const;
private:
    template <class Clock, class Duration>
    void doClockCast(std::chrono::time_point<Clock, Duration> &_rtp, const std::chrono::time_point<Clock, Duration> &_rmytp)const{
        _rtp = timePointCast<std::chrono::time_point<Clock, Duration>>();
    }
    
    template <class Clock, class MyClock, class Duration>
    void doClockCast(std::chrono::time_point<Clock, Duration> &_rtp, const std::chrono::time_point<MyClock, Duration> &_rmytp)const{
        const typename Clock::time_point other_now = Clock::now();                                                                                                                                             
        const typename MyClock::time_point my_now = MyClock::now();
        const typename MyClock::time_point my_tp = timePointCast<typename MyClock::time_point>();
        const auto delta = my_tp - my_now;
        _rtp = std::chrono::time_point_cast<Duration>(other_now + delta);
    }
};

int test_nanotime(int argc, char **argv){
    for(int i = 0; i < 1; ++i){
        auto steady_now = std::chrono::steady_clock::now();
        
        NanoTime    nano_now{steady_now};
        
        auto now2 = nano_now.timePointCast<std::chrono::steady_clock::time_point>();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        //nano_now -> system clock timepoint
        auto now3 = nano_now.timePointClockCast<std::chrono::system_clock::time_point, std::chrono::steady_clock>();
        
        NanoTime    system_now{now3};
        
        cout<<nano_now.tv_sec<<":"<<nano_now.tv_nsec<<endl;
        cout<<system_now.tv_sec<<":"<<system_now.tv_nsec<<endl;
        
        auto now4 = system_now.timePointClockCast<std::chrono::steady_clock::time_point, std::chrono::system_clock>();
        {
            NanoTime    n_now{now4};
            cout<<n_now.tv_sec<<":"<<n_now.tv_nsec<<endl;
        }
        auto now5 = system_now.timePointClockCast<std::chrono::system_clock::time_point, std::chrono::system_clock>();
        {
            NanoTime    n_now{now5};
            cout<<n_now.tv_sec<<":"<<n_now.tv_nsec<<endl;
        }
        cout<<endl;
        
        SOLID_CHECK(steady_now == now2);
        //SOLID_CHECK(steady_now == now4);
    }
    return 0;
}
