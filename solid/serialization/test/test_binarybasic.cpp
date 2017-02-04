#include "solid/serialization/binarybasic.hpp"
#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/algorithm.hpp"
#include "solid/utility/common.hpp"
#include <bitset>
#include <iostream>

using namespace std;
using namespace solid;
using namespace solid::serialization::binary;
//---

void print(uint32_t _v)
{
    bitset<32> bs(_v);
    cout << bs << ": max_bit_count = " << max_bit_count(_v);
    cout << " max_padded_bit_cout = " << max_padded_bit_cout(_v);
    cout << " max_padded_byte_cout = " << max_padded_byte_cout(_v) << endl;
}

//=============================================================================

//=============================================================================

template <typename T>
bool test(const T& _v, const size_t _estimated_size)
{
    char   tmp[256] = {0};
    size_t sz       = crossSize(_v);
    if (sz != _estimated_size) {
        SOLID_THROW("error");
        return false;
    }
    char* p = crossStore(tmp, _v);
    if (p == nullptr) {
        SOLID_THROW("error");
        return false;
    }

    if (static_cast<size_t>(p - tmp) != _estimated_size) {
        SOLID_THROW("error");
        return false;
    }

    if (crossSize(tmp) != _estimated_size) {
        SOLID_THROW("error");
        return false;
    }

    T v;

    const char* cp;

    cp = crossLoad(tmp, v);

    if (cp == nullptr) {
        SOLID_THROW("error");
        return false;
    }

    if (static_cast<size_t>(cp - tmp) != _estimated_size) {
        SOLID_THROW("error");
        return false;
    }

    if (v != _v) {
        SOLID_THROW("error");
        return false;
    }
    return true;
}

int test_binarybasic(int argc, char* argv[])
{
    cout << "max uint8_t value with crc: " << (int)max_value_without_crc_8() << endl;

    print(0);
    print(1);
    print(2);
    print(3);
    print(128);
    print(255);
    print(256);
    print(0xffff);
    print(0xfffff);
    print(0xffffff);
    print(0xfffffff);
    print(0xffffffff);

    if (!test(static_cast<uint8_t>(0x00), 1)) {
        SOLID_ASSERT(false);
    }

    if (!test(static_cast<uint8_t>(0x1), 2)) {
        SOLID_ASSERT(false);
    }

    if (!test(static_cast<uint8_t>(0xff), 2)) {
        SOLID_ASSERT(false);
    }

    if (!test(static_cast<uint16_t>(0), 1)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint16_t>(0xff), 2)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint16_t>(0xffff), 3)) {
        SOLID_ASSERT(false);
    }

    if (!test(static_cast<uint32_t>(0), 1)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint32_t>(0xff), 2)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint32_t>(0xffff), 3)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint32_t>(0xffffff), 4)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint32_t>(0xffffffff), 5)) {
        SOLID_ASSERT(false);
    }

    if (!test(static_cast<uint64_t>(0), 1)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xff), 2)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffff), 3)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffff), 4)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffffff), 5)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffffffffULL), 6)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffffffffffULL), 7)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffffffffffffULL), 8)) {
        SOLID_ASSERT(false);
    }
    if (!test(static_cast<uint64_t>(0xffffffffffffffffULL), 9)) {
        SOLID_ASSERT(false);
    }
    return 0;
}
