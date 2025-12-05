#include "solid/system/exception.hpp"
#include "solid/utility/sharedbuffer.hpp"
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;
using namespace solid;

int test_shared_buffer(int argc, char* argv[])
{
    {
        SharedBuffer sb = make_shared_buffer(1000);

        cout << sb.capacity() << endl;
        cout << sb.size() << endl;

        sb.resize(100);

        solid_check(sb.size() == 100);
        {
            SharedBuffer sb2 = sb;

            cout << "sb.usecount = " << sb2.useCount() << endl;
        }
        cout << "exiting..." << endl;
        return 0;
        // solid_check(sb2.size() == 100);

        // SharedBuffer sb3 = sb2; // sb3 == sb2

        // solid_check(sb3);
        // cout << "exiting..." << endl;
    }
    {
        cout << "exiting..." << endl;
    }
    {
        MutableSharedBuffer sb = make_mutable_buffer(1000);

        cout << sb.capacity() << endl;
        cout << sb.msize() << endl;
        string_view pangram = "the quick brown fox jumps over the lazy dog";
        strncpy(sb.mdata(), pangram.data(), sb.capacity());

        sb.append(pangram.size());

        sb.mdata()[0] = 'T';

        solid_check(sb.msize() == pangram.size());

        // MutableSharedBuffer sbxx{sb};//will not compile

        MutableSharedBuffer sbx{std::move(sb)};

        solid_check(sbx.msize() == pangram.size());
        solid_check(!sb);

        ConstSharedBuffer csb = std::move(sbx);

        solid_check(csb.size() == pangram.size());

        // csb.data()[0] = 't';//will not compile

        ConstSharedBuffer csb2 = csb; // sb3 == sb2

        solid_check(csb.size() == pangram.size());
        auto sbc = csb.collapse();
        solid_check(!sbc);
        sbc = csb2.collapse();
        solid_check(sbc);
        solid_check(!csb2);
        solid_check(sbc.msize() == pangram.size());
        cout << "Data: " << sbc.mdata() << endl;
    }
    return 0;
}