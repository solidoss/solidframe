#include <iostream>
#include "system/common.hpp"
#include <system/atomic.hpp>

#include "system/exception.hpp"
#include <vector>

#include "serialization/binary.hpp"

using namespace std;
using namespace solid;

using solid::serialization::binary::ExtendedData;


int test_extdata(int argc, char *argv[]){
	ExtendedData 	e1(static_cast<uint32>(10));
	cout<<e1.first_uint32_value()<<endl;
	
	e1.generic(std::string("ceva"));
	
	cout<<*e1.genericCast<std::string>()<<endl;
	
	SOLID_CHECK(e1.genericCast<std::vector<int>>() == nullptr);
	
	return 0;
}
