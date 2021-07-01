#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/any.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

//#define TEST_BOOST_ANY

#ifdef TEST_BOOST_ANY
#include "boost/any.hpp"
#endif

using namespace solid;
using namespace std;

struct TestNoCopy {
    std::string str;

    TestNoCopy(const std::string& _rstr)
        : str(_rstr)
    {
    }

    TestNoCopy(const TestNoCopy&) = delete;
    TestNoCopy& operator=(const TestNoCopy&) = delete;

    TestNoCopy(TestNoCopy&& _utnc) noexcept
        : str(std::move(_utnc.str))
    {
    }

    TestNoCopy& operator=(TestNoCopy&& _utnc) noexcept
    {
        str = std::move(_utnc.str);
        return *this;
    }
};

#ifdef TEST_BOOST_ANY
std::string test_boost_any(const boost::any& _rany)
{
    boost::any  any0(_rany);
    std::string retval;
    TestNoCopy* p = boost::any_cast<TestNoCopy*>(any0);
    if (p) {
        retval = p->str;
    }
    return retval;
}
#endif

void test_any_no_copy_copy(const Any<32>& _rany)
{
    bool caught_exception = false;
    try {
        Any<32> tmp_any(_rany);
        solid_check(!tmp_any.has_value());
        tmp_any.cast<TestNoCopy>()->str.clear();
    } catch (std::exception& rex) {
        cout << "Exception: " << rex.what() << endl;
        caught_exception = true;
    }
    solid_check(_rany.has_value());
    solid_check(caught_exception);
    caught_exception = false;

    Any<32> tmp_any;

    try {
        tmp_any = _rany;
    } catch (std::exception& rex) {
        cout << "Exception: " << rex.what() << endl;
        caught_exception = true;
    }

    solid_check(!tmp_any.has_value());
    solid_check(_rany.has_value());
    solid_check(caught_exception);
}

std::string test_any_no_copy_move(Any<32>& _rany)
{
    Any<32> tmp_any(std::move(_rany));

    solid_check(!_rany.has_value() || (_rany.cast<TestNoCopy>() && _rany.cast<TestNoCopy>()->str.empty()));
    solid_check(tmp_any.has_value() && tmp_any.cast<TestNoCopy>() != nullptr && !tmp_any.cast<TestNoCopy>()->str.empty());

    TestNoCopy* p = tmp_any.cast<TestNoCopy>();

    solid_check(p != nullptr);
    return p->str;
}

struct Data {
    Data()
        : age(0)
    {
    }
    Data(const std::string& _name, const std::string& _address, uint16_t _age)
        : name(_name)
        , address(_address)
        , age(_age)
    {
    }

    std::string name;
    std::string address;
    uint16_t    age;
};

struct Test {
    int i_;

    Test(int _i)
        : i_(_i)
    {
        cout << "Test(): " << this << " : " << i_ << endl;
    }

    Test(const Test& _t)
        : i_(_t.i_)
    {
        cout << "Test(const Test&): " << this << " : " << i_ << endl;
    }

    Test(Test&& _t) noexcept
        : i_(_t.i_)
    {
        _t.i_ = 0;
        cout << "Test(Test&&): " << this << " : " << i_ << endl;
    }

    Test& operator=(const Test& _t) = delete;
    Test& operator=(Test&& _t) = delete;

    ~Test()
    {
        cout << "~Test(): " << this << " : " << i_ << endl;
    }
};

int test_any(int /*argc*/, char* /*argv*/[])
{
#ifdef TEST_BOOST_ANY
    {
        boost::any any0(TestNoCopy("a string"));

        TestNoCopy* p = boost::any_cast<TestNoCopy*>(any0);
        if (p) {
            cout << "p->str = " << p->str << endl;
        } else {
            cout << "p == nullptr" << endl;
        }
        cout << "copy string = " << test_boost_any(any0) << endl;
    }

#endif

    Any<> any0;
    auto  any32(make_any<string, 32>(string("best string ever")));

    cout << "sizeof(any0) = " << sizeof(any0) << endl;

    solid_check(any32.has_value());
    solid_check(any32.cast<string>() != nullptr);
    solid_check(any32.get_if<string>() != nullptr);
    solid_check(any32.cast<int>() == nullptr);
    solid_check(any32.get_if<int>() == nullptr);

    cout << "value = " << *any32.cast<string>() << endl;

    any0 = std::move(any32);

    solid_check(!any32.has_value() || (any32.cast<string>() != nullptr && any32.cast<string>()->empty()));
    solid_check(any0.has_value());

    solid_check(any0.cast<string>() != nullptr);
    solid_check(any0.cast<int>() == nullptr);

    cout << "value = " << *any0.cast<string>() << endl;

    any32 = any0;

    solid_check(any32.has_value());
    solid_check(any0.has_value());

    Any<16> any16_0(any32);
    Any<16> any16_1(any16_0);

    solid_check(any32.has_value());
    solid_check(any16_0.has_value());
    solid_check(any16_1.has_value());

    solid_check(*any16_1.cast<string>() == *any32.cast<string>() && *any16_1.cast<string>() == *any16_0.cast<string>());

    Any<16> any16_2(std::move(any16_0));

    solid_check(!any16_0.has_value());
    solid_check(any16_2.has_value());

    solid_check(*any16_2.cast<string>() == *any32.cast<string>());
    solid_check(*any16_2.get_if<string>() == *any32.get_if<string>());

    auto any_nc_0(make_any<TestNoCopy, 32>("a string"));

    test_any_no_copy_copy(any_nc_0);

    cout << "test_any_no_copy_move: " << test_any_no_copy_move(any_nc_0) << endl;

    {
        std::shared_ptr<Data> ptr = std::make_shared<Data>("gigel", "gigel@viscol.ro", 33);

        cout << "ptr.get = " << ptr.get() << endl;

        auto any_ptr1 = make_any<std::shared_ptr<Data>, 256>(ptr);

        cout << "any_ptr1->get = " << any_ptr1.cast<std::shared_ptr<Data>>()->get() << endl;

        Any<256> any_ptr2 = any_ptr1;

        cout << "any_ptr1->get = " << any_ptr1.cast<std::shared_ptr<Data>>()->get() << endl;
        cout << "any_ptr2->get = " << any_ptr2.cast<std::shared_ptr<Data>>()->get() << endl;

        cout << "ptr.get = " << ptr.get() << endl;
        cout << "ptr usecount = " << ptr.use_count() << endl;
        solid_check(ptr.use_count() == 3);
        solid_check(ptr.get() == any_ptr1.cast<std::shared_ptr<Data>>()->get() && any_ptr2.cast<std::shared_ptr<Data>>()->get() == ptr.get());

        auto any_ptr3 = make_any<std::shared_ptr<Data>, 256>(std::move(ptr));

        solid_check(any_ptr2.cast<std::shared_ptr<Data>>()->get() == any_ptr2.cast<std::shared_ptr<Data>>()->get());
        solid_check(any_ptr2.cast<std::shared_ptr<Data>>()->use_count() == 3);
        solid_check(ptr.use_count() == 0);
    }
    {
        Any<sizeof(Test)> any_t{Test{10}};
        Any<>             any_0{std::move(any_t)};

        solid_check(any_t.has_value());
        solid_check(any_0.has_value());

        any_t = std::move(any_0);

        solid_check(any_0.has_value());
        solid_check(any_t.has_value());

        any_0 = Test{5};
        solid_check(any_0.has_value());
    }
    {

        using Array4T = std::array<size_t, 4>;
        Any<8> any{Array4T{{1, 2, 3, 4}}};
        solid_check((*any.cast<Array4T>())[0] == 1);
        solid_check((*any.cast<Array4T>())[1] == 2);
        solid_check((*any.cast<Array4T>())[2] == 3);
        solid_check((*any.cast<Array4T>())[3] == 4);
        (*any.cast<Array4T>())[3] = 10;
        solid_check((*any.cast<Array4T>())[3] == 10);
    }
    {

        using Array4T = std::array<size_t, 4>;
        Any<128> any(Array4T{{1, 2, 3, 4}});
        solid_check((*any.cast<Array4T>())[0] == 1);
        solid_check((*any.cast<Array4T>())[1] == 2);
        solid_check((*any.cast<Array4T>())[2] == 3);
        solid_check((*any.cast<Array4T>())[3] == 4);
        solid_check((*any.get_if<Array4T>())[3] == 4);
        (*any.cast<Array4T>())[3] = 10;
        solid_check((*any.cast<Array4T>())[3] == 10);
    }
    {

        using Array4T = std::array<size_t, 4>;
        Any<128> any;
        any = Array4T{{1, 2, 3, 4}};
        solid_check((*any.cast<Array4T>())[0] == 1);
        solid_check((*any.cast<Array4T>())[1] == 2);
        solid_check((*any.cast<Array4T>())[2] == 3);
        solid_check((*any.cast<Array4T>())[3] == 4);
        (*any.cast<Array4T>())[3] = 10;
        solid_check((*any.cast<Array4T>())[3] == 10);
    }
    {
        using Array4T = std::array<size_t, 4>;
        Array4T arr{{1, 2, 3, 4}};
        auto    lambda = [arr](const char* /*_txt*/) mutable {
            solid_check(arr[3] == 4);
            arr[3] = 10;
        };
        Any<128> any;
        any = lambda;
        (*any.cast<decltype(lambda)>())("Test");
    }
    {
        using Array4T = std::array<size_t, 4>;
        Array4T arr{{1, 2, 3, 4}};
        auto    lambda = [arr](const char* /*_txt*/) mutable {
            solid_check(arr[3] == 4);
            arr[3] = 10;
        };
        Any<128> any(lambda);

        (*any.cast<decltype(lambda)>())("Test");
    }
    {
        using Array4T = std::array<size_t, 4>;
        Array4T       arr{{1, 2, 3, 4}};
        std::ifstream ifs;
        auto          lambda = [arr, ifs = std::move(ifs)](const char* /*_txt*/) mutable {
            solid_check(arr[3] == 4);
            arr[3] = 10;
        };
        Any<128> any(std::move(lambda));

        (*any.cast<decltype(lambda)>())("Test");
    }
    return 0;
}
