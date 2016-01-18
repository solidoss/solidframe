#include "utility/any.hpp"
#include "system/cassert.hpp"
#include <string>
#include <iostream>

using namespace solid;
using namespace std;

int test_any(int argc, char *argv[]){
	Any<>		any0;
	Any<32>		any32(string("best string ever"));
	Any<16>		any16(any32);
	
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
	
	return 0;
}