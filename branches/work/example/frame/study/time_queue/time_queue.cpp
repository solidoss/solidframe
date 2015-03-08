#include <iostream>
#include "system/timespec.hpp"
#include "utility/timerqueue.hpp"

using namespace std;
using namespace solid;


uint64		v = 0;

void do_something(size_t const &_rv){
	v += _rv;
}

typedef void (*FunctionT)(size_t const &);

size_t addTimeCount(const size_t _i){
	static const size_t t[] = {10, 20, 30, 40, 50, 40, 30, 20, 10};
	return t[_i % (sizeof(t)/sizeof(size_t))];
}

TimeSpec newTime(const TimeSpec &_crttime, const size_t _j){
	static const size_t	t[] = {1000, 2000, 3000, 4000, 5000, 6000, 3500, 7000, 2500, 8000, 1500, 9000, 500};
	TimeSpec 			ts = _crttime;
	ts += t[_j % (sizeof(t)/sizeof(size_t))];
	return ts;
}

void newCurrentTime(TimeSpec &_crttime, const size_t _i){
	static const size_t	t[] = {900, 1900, 2900, 3900, 4900, 5900, 6900, 7900, 8900, 9900};
	_crttime += t[_i % (sizeof(t)/sizeof(size_t))];
}

template <class TQ>
void time_test(
	const size_t _repeatcnt,
	const size_t _loopcnt
){
	TQ			tq;
	for(size_t k = 0; k < _repeatcnt; ++k){
		TimeSpec	crttime = TimeSpec::createMonotonic();
		
		for(size_t i = 0; i < _loopcnt; ++i){
			const size_t add_cnt = addTimeCount(i);
			for(size_t j = 0; j < add_cnt; ++j){
				tq.push(newTime(crttime, j), i);
			}
			newCurrentTime(crttime, i);
			tq.pop(crttime, do_something);
		}
		
		tq.pop(TimeSpec::maximum, do_something);
	}
}

///////////////////////////////////////////////////////////////////////////////
class PriorityTimeQueue{
public:
	void push(TimeSpec const &_ts, size_t _v){
		
	}
	
	void pop(TimeSpec const &_ts, FunctionT _pf){
		
	}
private:
	
};
///////////////////////////////////////////////////////////////////////////////
class IterateTimeQueue{
public:
	void push(TimeSpec const &_ts, size_t _v){
		
	}
	
	void pop(TimeSpec const &_ts, FunctionT _pf){
		
	}
private:
	
};
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
	size_t	repeatcnt = 1000;
	size_t	loopcnt = 10000;
	int		choice = 0;
	
	switch(choice){
		case 0:
			time_test<PriorityTimeQueue>(repeatcnt, loopcnt);
			break;
		case 1:
			time_test<IterateTimeQueue>(repeatcnt, loopcnt);
			break;
		default:
			cout<<"Unknown choice: "<<choice<<endl;
			break;
	}
	return 0;
}