#include "solid/system/exception.hpp"
#include "solid/utility/sharedbuffer.hpp"
#include <iostream>

using namespace std;
using namespace solid;

int test_shared_buffer(int argc, char* argv[])
{
    SharedBuffer sb = make_shared_buffer(1000);

    cout << sb.capacity() << endl;
    cout << sb.size() << endl;

    sb.resize(100);

    solid_check(sb.size() == 100);

    SharedBuffer sb2 = sb;

    solid_check(sb2.size() == 100);

    for (size_t i = 8; i <= 1024; i += 8) {
        SharedBuffer sb = make_shared_buffer(i);
        cout << i << "\t" << sb.capacity() << endl;
    }

    return 0;
}