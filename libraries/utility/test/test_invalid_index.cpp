#include "utility/common.hpp"
#include <cassert>
#include <iostream>

using namespace std;

using namespace solid;

int test_invalid_index(int argc, char *argv[]){
	
	size_t sz = InvalidIndex();
	
	cout <<"sz = "<<sz<<endl;
	
	assert(sz == static_cast<size_t>(-1));
	assert(sz == InvalidIndex());
	
	uint64 sz64 = InvalidIndex();
	
	assert(sz64 == -1LL);
	
	cout <<"sz64 = "<<sz64<<" "<<static_cast<uint64>(-1LL)<<endl;
	
	
	
	return 0;
}