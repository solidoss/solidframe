#include "solid/utility/common.hpp"
#include <cassert>
#include <iostream>

using namespace std;

using namespace solid;

int test_invalid_index(int argc, char *argv[]){

    size_t sz = InvalidIndex();

    cout <<"sz = "<<sz<<endl;

    assert(sz == static_cast<size_t>(-1));
    assert(sz == InvalidIndex());

    uint64_t sz64 = InvalidIndex();

    assert(sz64 == -1ULL);

    cout <<"sz64 = "<<sz64<<" "<<static_cast<uint64_t>(-1LL)<<endl;



    return 0;
}