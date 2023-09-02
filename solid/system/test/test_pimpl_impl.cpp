#include "test_pimpl.hpp"
#include <atomic>
#include <iostream>
using namespace std;

struct Test::Data {
    std::atomic<uint64_t> v1_{0};
    string                v2_;

    Data(int _v1, const string_view& _v2)
        : v1_(_v1)
        , v2_(_v2)
    {
    }
    Data() = default;

    Data(const Data& _other)
        : v1_(_other.v1_.load())
        , v2_(_other.v2_)
    {
        cout << "Data copy" << endl;
    }

    Data(Data&& _other)
        : v1_(_other.v1_.load())
        , v2_(std::move(_other.v2_))
    {
        cout << "Data move" << endl;
    }

    ~Data()
    {
        cout << "~Data" << endl;
    }
};

Test::Test() {}
Test::Test(int _v1, const std::string_view& _v2)
    : pimpl_(_v1, _v2)
{
}
Test::~Test() {}

Test::Test(const Test&) = default;
Test::Test(Test&&)      = default;

std::ostream& Test::print(std::ostream& _ros) const
{
    _ros << "v1 = " << pimpl_->v1_ << " v2 = " << pimpl_->v2_ << endl;
    return _ros;
}