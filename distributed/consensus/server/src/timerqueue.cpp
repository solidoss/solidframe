#include "timerqueue.hpp"

bool TimerQueue::TimerDataCmp::operator()(
	const TimerData &_rtd1, const TimerData &_rtd2
)const{
	return _rtd1.timepos > _rtd2.timepos;
}


TimerQueue::TimerQueue(){}

void TimerQueue::push(const TimeSpec &_rts, uint32 _idx, uint16 _uid, uint16 _v){
	tq.push(TimerData(_rts, _idx, _uid, _v));
}
