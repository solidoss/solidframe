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

        SharedBuffer sb2 = sb;

        solid_check(sb2.size() == 100);

        for (size_t i = 8; i <= 1024; i += 8) {
            SharedBuffer sb = make_shared_buffer(i);
            cout << i << "\t" << sb.capacity() << endl;
        }

        SharedBuffer sb3 = sb2; // sb3 == sb2

        solid_check(!sb2.resurrect());
        solid_check(!sb.resurrect());

        solid_check(sb3.resurrect());
        solid_check(sb3);
    }
    {
        std::promise<SharedBuffer> p;
        std::future<SharedBuffer>  f = p.get_future();

        vector<thread> thr_vec;
        void*          psb1 = nullptr;
        {

            SharedBuffer sb1 = BufferManager::make(1000);
            SharedBuffer sb2 = BufferManager::make(2000);

            cout << static_cast<void*>(sb1.data()) << endl;
            cout << static_cast<void*>(sb2.data()) << endl;

            psb1 = sb1.data();

            solid_check(psb1);

            auto lambda = [&p, sb1, sb2]() mutable {
                this_thread::sleep_for(chrono::seconds(1));
                if (sb1.resurrect()) {
                    p.set_value(std::move(sb1));
                }
                if (sb2.resurrect()) {
                }
            };

            for (size_t i = 0; i < 4; ++i) {
                thr_vec.emplace_back(lambda);
            }
        }
        {
            auto sb = f.get();

            cout << static_cast<void*>(sb.data()) << endl;
            solid_check(psb1 == static_cast<void*>(sb.data()));
        }
        {
            SharedBuffer sb1 = BufferManager::make(1000);
            cout << static_cast<void*>(sb1.data()) << endl;
            solid_check(psb1 == static_cast<void*>(sb1.data()));
        }
        for (auto& rt : thr_vec) {
            rt.join();
        }
    }
    return 0;
}