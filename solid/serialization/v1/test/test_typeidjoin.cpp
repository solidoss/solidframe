#include "solid/serialization/v1/binarybasic.hpp"
#include "solid/serialization/v1/typeidmap.hpp"
#include "solid/system/exception.hpp"
#include <iostream>

using namespace std;
using namespace solid;
using namespace solid::serialization;

bool test(const uint32_t _protocol_id, const uint64_t _message_id, bool _should_fail_join = false)
{

    uint64_t tid;

    if (!joinTypeId(tid, _protocol_id, _message_id) and !_should_fail_join) {
        SOLID_THROW("fail join");
        return false;
    } else if (_should_fail_join) {
        return true;
    }

    cout << "cross::size(" << tid << ") = " << binary::cross::size(tid) << endl;

    uint32_t pid;
    uint64_t mid;

    splitTypeId(tid, pid, mid);

    if (mid == _message_id and pid == _protocol_id) {
        return true;
    } else {
        SOLID_THROW("fail split");
        return false;
    }
}

int test_typeidjoin(int argc, char* argv[])
{

    test(0, 0);
    test(1, 1);
    test(2, 2);
    test(0xff, 0xff);
    test(0xffff, 0xffff);
    test(0xffffff, 0xffffff);
    test(0xffffffff, 0xffffffff, true);
    test(0xffff, 0xffffffffff);
    return 0;
}