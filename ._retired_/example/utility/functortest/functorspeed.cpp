#include "system/debug.hpp"
#include "system/common.hpp"
#include "system/function.hpp"
#include "utility/functor.hpp"

#include <iostream>
#include <string>
#include <deque>

using namespace std;
using namespace solid;


typedef std::deque<std::string>		StringDequeT;


const char *	strings[] = {
	"something",
	"somethingsomething",
	"somethingsomethingsomething",
	"somethingsomethingsomethingsomething",
	"somethingsomethingsomethingsomethingsomething",
	"somethingsomethingsomethingsomethingsomethingsomething",
	"somethingsomethingsomethingsomethingsomethingsomethingsomething",
	"somethingsomethingsomethingsomethingsomethingsomethingsomethingsomething",
	"somethingsomethingsomethingsomethingsomethingsomethingsomethingsomethingsomething"
};

typedef FunctorReference<void, std::string const&>	FunctorReferenceT;

typedef FUNCTION<void(std::string const&)>			FunctionT;

struct CountStringSize{
	CountStringSize():sz(0){}
	uint64		sz;
	
	void operator()(std::string const& _rstr){
		sz += _rstr.size();
	}
};

int main(int argc, char *argv[]){
	size_t			item_count = 1000000;
	size_t			repeat_count = 10;
	
	const size_t	string_count = sizeof(strings)/sizeof(const char*);
	StringDequeT	stringdq;
	
	for(size_t i = 0; i < item_count; ++i){
		stringdq.push_back(strings[i % string_count]);
	}
#if 0
	CountStringSize 	cs;
	
	for(size_t j = 0; j < repeat_count; ++j){
		FunctorReferenceT	csf(cs);
		
		for(auto it = stringdq.begin(); it != stringdq.end(); ++it){
			csf(*it);
		}
		
	}
	
	cout<<cs.sz<<endl;
#else
	uint64		sz(0);
	for(size_t j = 0; j < repeat_count; ++j){
		FunctionT	f(
			[&sz](std::string const& _rstr){
				sz += _rstr.size();
			}
		);
		
		for(auto it = stringdq.begin(); it != stringdq.end(); ++it){
			f(*it);
		}
		
	}
	
	cout<<sz<<endl;
#endif
	return 0;
}
