#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <thread>
#include <memory>
#include <future>

#include "solid/system/exception.hpp"
#include "solid/utility/common.hpp"
using namespace std;

struct Message{
    std::string text_;
};
using SharedMessageT = std::shared_ptr<Message>;

int test_collapse(int argc, char *argv[])
{
    {
        std::promise<SharedMessageT> p;
        auto  f = p.get_future();
        auto sm = std::make_shared<Message>();
        vector<jthread> thr_vec;
        {
            auto lambda = [&p](SharedMessageT _sm) mutable {
                if (auto tmp_sm = solid::collapse(_sm)) {
                    p.set_value(std::move(tmp_sm));
                }
            };

            auto sm_lock{sm};
            for (size_t i = 0; i < 40; ++i) {
                thr_vec.emplace_back(lambda, sm);
            }

            if (auto tmp_sm = solid::collapse(sm_lock)) {
                p.set_value(std::move(tmp_sm));
            }
        }
        {
            auto lsm = f.get();
            cout<<"done "<<lsm.get()<<' '<<sm.get()<<endl;
            solid_check(lsm.get() == sm.get());
        }
    }
    return 0;
}
