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
        string_view pangram = "the quick brown fox jumps over the lazy dog";
        strncpy(sb.data(), pangram.data(), sb.capacity());

        sb.resize(pangram.size());

        sb.data()[0] = 'T';

        solid_check(sb.size() == pangram.size());

        // SharedBuffer sbx{sb};//will not compile

        SharedBuffer sbx{std::move(sb)};

        solid_check(sbx.size() == pangram.size());
        solid_check(!sb);

        ConstSharedBuffer csb = move(sbx);

        solid_check(csb.size() == pangram.size());

        // csb.data()[0] = 't';//will not compile

        ConstSharedBuffer csb2 = csb; // sb3 == sb2

        solid_check(csb.size() == pangram.size());
        auto sbc = csb.collapse();
        solid_check(!sbc);
        sbc = csb2.collapse();
        solid_check(sbc);
        solid_check(!csb2);
        solid_check(sbc.size() == pangram.size());
        cout << "Data: " << sbc.data() << endl;
    }
    {
        std::promise<SharedBuffer> p;
        std::future<SharedBuffer>  f = p.get_future();

        vector<thread> thr_vec;
        const void*    psb1 = nullptr;
        {

            ConstSharedBuffer csb1 = BufferManager::make(1000);
            ConstSharedBuffer csb2 = BufferManager::make(2000);

            cout << static_cast<const void*>(csb1.data()) << endl;
            cout << static_cast<const void*>(csb2.data()) << endl;

            psb1 = csb1.data();

            solid_check(psb1);

            auto lambda = [&p, csb1, csb2]() mutable {
                this_thread::sleep_for(chrono::milliseconds(200));
                auto sbc = csb1.collapse();
                if (sbc) {
                    p.set_value(std::move(sbc));
                }
                sbc = csb2.collapse();
                if (sbc) {
                    solid_check(!csb2);
                    solid_check(BufferManager::localCount(2000) == 0);
                }
            };

            for (size_t i = 0; i < 4; ++i) {
                thr_vec.emplace_back(lambda);
            }
            auto sbc = csb1.collapse();
            if (sbc) {
                p.set_value(std::move(sbc));
            }
        }

        for (auto& t : thr_vec) {
            t.join();
        }
        solid_check(BufferManager::localCount(1000) == 0);
        {
            auto sb = f.get();

            cout << static_cast<void*>(sb.data()) << endl;
            solid_check(psb1 == static_cast<void*>(sb.data()));
        }
        solid_check(BufferManager::localCount(1000) == 1);
        {
            SharedBuffer sb1 = BufferManager::make(1000);
            cout << static_cast<void*>(sb1.data()) << endl;
            solid_check(BufferManager::localCount(1000) == 0);
            solid_check(psb1 == static_cast<void*>(sb1.data()));
        }
        solid_check(BufferManager::localCount(1000) == 1);
    }

    for (size_t i = 100; i < 4000; i += 100) {
        SharedBuffer sb1 = make_shared_buffer(i);
        SharedBuffer sb2 = BufferManager::make(i);
        solid_check(sb1.capacity() == i);
        solid_check(sb2.capacity() == i);
        cout << i << " " << sb1.capacity() << " " << sb1.actualCapacity() << " " << sb2.capacity() << " " << sb2.actualCapacity() << endl;
    }
    {
        SharedBuffer empty_buf;
        solid_check(empty_buf.capacity() == 0);
        cout << "Empty buffer actualCapacity = " << empty_buf.actualCapacity() << endl;
        solid_check(empty_buf.actualCapacity() == 0);
    }
    {
        SharedBuffer zero_buf = make_shared_buffer(0);
        solid_check(zero_buf.capacity() == 0);
        cout << "Zero buffer actualCapacity = " << zero_buf.actualCapacity() << endl;
        solid_check(zero_buf.actualCapacity() != 0 && zero_buf.actualCapacity() < 0xffffffff);
    }
    return 0;
}