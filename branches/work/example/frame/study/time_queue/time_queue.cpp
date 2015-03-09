#include <iostream>
#include "system/timespec.hpp"
#include "utility/timerqueue.hpp"

using namespace std;
using namespace solid;


uint64		v = 0;
size_t		maxsz = 0;
size_t		skipcnt = 0;
size_t		emptycnt = 0;

void do_something(size_t const &_rv){
	v += _rv;
}

typedef void (*FunctionT)(size_t const &);

size_t addTimeCount(const size_t _i){
	static const size_t t[] = {100, 200, 300, 400, 500, 400, 300, 200, 100};
	return t[_i % (sizeof(t)/sizeof(size_t))];
}

TimeSpec newTime(const TimeSpec &_crttime, const size_t _j){
	static const size_t	t[] = {1000, 2000, 3000, 4000, 5000, 6000, 3500, 7000, 2500, 8000, 1500, 9000, 500};
	TimeSpec 			ts = _crttime;
	ts += t[_j % (sizeof(t)/sizeof(size_t))];
	return ts;
}

void newCurrentTime(TimeSpec &_crttime, const size_t _i){
	//static const size_t	t[] = {900, 1900, 2900, 3900, 4900, 5900, 6900, 7900, 8900, 9900};
	static const size_t	t[] = {900, 0, 0, 0, 0, 900, 1900, 2900, 3900, 4900};
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
		tq.push(_ts, _v);
		if(tq.size() > maxsz){
			maxsz = tq.size();
		}
	}
	
	void pop(TimeSpec const &_ts, FunctionT _pf){
		while(tq.isHit(_ts)){
			(*_pf)(tq.frontValue());
			tq.pop();
		}
	}
private:
	TimerQueue<size_t>		tq;
};
///////////////////////////////////////////////////////////////////////////////
#define USE_MINTS
class IterateTimeQueue{
public:
	IterateTimeQueue(){
		mints = TimeSpec::maximum;
	}
	void push(TimeSpec const &_ts, size_t _v){
		vv.push_back(ValueT(_ts, _v));
		if(vv.size() > maxsz){
			maxsz = vv.size();
		}
#ifdef USE_MINTS
		if(_ts < mints){
			mints = _ts;
		}
#endif
	}
	
	void pop(TimeSpec const &_ts, FunctionT _pf){
#ifdef USE_MINTS
		if(_ts < mints){
			++skipcnt;
			return;
		}
#endif
		mints = TimeSpec::maximum;
		
		size_t		sz = vv.size();
		bool		pass  = false;
		
		for(size_t i = 0; i < sz;){
			ValueT const & rv(vv[i]);
			if(rv.first > _ts){
#ifdef USE_MINTS
				if(rv.first < mints){
					mints = rv.first;
				}
#endif
				++i;
			}else{
				pass  = true;
				--sz;
				(*_pf)(rv.second);
				vv[i] = vv.back();
				vv.pop_back();
			}
		}
		
		if(!pass){
			++emptycnt;
		}
		//vv.resize(sz);
	}
private:
	typedef std::pair<TimeSpec, size_t>		ValueT;
	typedef std::vector<ValueT>				ValueVectorT;
	ValueVectorT		vv;
	TimeSpec			mints;
};
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
	int		choice = 0;
	size_t	repeatcnt = 500;
	size_t	loopcnt = 10000;
	
	if(argc > 1){
		choice = atoi(argv[1]);
	}
	if(argc > 2){
		repeatcnt = atoi(argv[2]);
	}
	
	if(argc > 2){
		loopcnt = atoi(argv[2]);
	}
	
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
	cout<<"V = "<<v<<" maxsz = "<<maxsz<<" skipcnt = "<<skipcnt<<" emptycnt = "<<emptycnt<<endl;
	return 0;
}