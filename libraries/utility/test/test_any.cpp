#include "utility/any.hpp"
#include "system/cassert.hpp"
#include <string>
#include <iostream>

using namespace solid;
using namespace std;

int test_any(int argc, char *argv[]){
	Any<>		any0;
	Any<32>		any32(any_create<32>(string("best string ever")));
	
	cout<<"sizeof(any0) = "<<sizeof(any0)<<endl;
	
	cassert(not any32.empty());
	cassert(any32.cast<string>() != nullptr);
	cassert(any32.cast<int>() == nullptr);
	
	cout<<"value = "<<*any32.cast<string>()<<endl;
	
	
	any0 = std::move(any32);
	
	cassert(any32.empty());
	cassert(not any0.empty());
	
	cassert(any0.cast<string>() != nullptr);
	cassert(any0.cast<int>() == nullptr);
	
	cout<<"value = "<<*any0.cast<string>()<<endl;
	
	any32 = any0;
	
	cassert(not any32.empty());
	cassert(not any0.empty());
	
	Any<16>		any16_0(any32);
	Any<16>		any16_1(any16_0);
	
	cassert(not any32.empty());
	cassert(not any16_0.empty());
	cassert(not any16_1.empty());
	
	cassert(*any16_1.cast<string>() == *any32.cast<string>() && *any16_1.cast<string>() == *any16_0.cast<string>());
	
	Any<16>		any16_2(std::move(any16_0));
	
	cassert(any16_0.empty());
	cassert(not any16_2.empty());
	
	
	cassert(*any16_2.cast<string>() == *any32.cast<string>());
	
	return 0;
}