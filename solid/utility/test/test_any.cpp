#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/any.hpp"
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

    TestNoCopy(TestNoCopy&& _utnc)
        : str(std::move(_utnc.str))
    {
    }

    TestNoCopy& operator=(TestNoCopy&& _utnc)
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
    try {
        Any<32> tmp_any(_rany);
        SOLID_CHECK(tmp_any.empty());
    } catch (std::exception& rex) {
        cout << "Exception: " << rex.what() << endl;
    }
    SOLID_CHECK(not _rany.empty());

    Any<32> tmp_any;

    bool caught_exception = false;

    try {
        tmp_any = _rany;
    } catch (std::exception& rex) {
        cout << "Exception: " << rex.what() << endl;
        caught_exception = true;
    }

    SOLID_CHECK(tmp_any.empty());
    SOLID_CHECK(not _rany.empty());
    SOLID_CHECK(caught_exception);
}

std::string test_any_no_copy_move(Any<32>& _rany)
{
    Any<32> tmp_any(std::move(_rany));

    SOLID_CHECK(_rany.empty());
    SOLID_CHECK(!tmp_any.empty());

    TestNoCopy* p = tmp_any.cast<TestNoCopy>();

    SOLID_CHECK(p != nullptr);
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

    Test(Test&& _t)
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

int test_any(int argc, char* argv[])
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

    cout << "is convertible: " << std::is_convertible<typename std::remove_reference<Any<>>::type*, AnyBase*>::value << endl;

    Any<>   any0;
    Any<32> any32(make_any<32, string>(string("best string ever")));

    cout << "sizeof(any0) = " << sizeof(any0) << endl;

    SOLID_CHECK(not any32.empty());
    SOLID_CHECK(any32.cast<string>() != nullptr);
    SOLID_CHECK(any32.cast<int>() == nullptr);

    cout << "value = " << *any32.cast<string>() << endl;

    any0 = std::move(any32);

    SOLID_CHECK(any32.empty());
    SOLID_CHECK(not any0.empty());

    SOLID_CHECK(any0.cast<string>() != nullptr);
    SOLID_CHECK(any0.cast<int>() == nullptr);

    cout << "value = " << *any0.cast<string>() << endl;

    any32 = any0;

    SOLID_CHECK(not any32.empty());
    SOLID_CHECK(not any0.empty());

    Any<16> any16_0(any32);
    Any<16> any16_1(any16_0);

    SOLID_CHECK(not any32.empty());
    SOLID_CHECK(not any16_0.empty());
    SOLID_CHECK(not any16_1.empty());

    SOLID_CHECK(*any16_1.cast<string>() == *any32.cast<string>() && *any16_1.cast<string>() == *any16_0.cast<string>());

    Any<16> any16_2(std::move(any16_0));

    SOLID_CHECK(any16_0.empty());
    SOLID_CHECK(not any16_2.empty());

    SOLID_CHECK(*any16_2.cast<string>() == *any32.cast<string>());

    Any<32> any_nc_0(make_any<32, TestNoCopy>("a string"));

    test_any_no_copy_copy(any_nc_0);

    cout << "test_any_no_copy_move: " << test_any_no_copy_move(any_nc_0) << endl;

    {
        std::shared_ptr<Data> ptr = std::make_shared<Data>("gigel", "gigel@viscol.ro", 33);

        cout << "ptr.get = " << ptr.get() << endl;

        Any<256> any_ptr1 = make_any<256, std::shared_ptr<Data>>(ptr);

        cout << "any_ptr1->get = " << any_ptr1.cast<std::shared_ptr<Data>>()->get() << endl;

        Any<256> any_ptr2 = any_ptr1;

        cout << "any_ptr1->get = " << any_ptr1.cast<std::shared_ptr<Data>>()->get() << endl;
        cout << "any_ptr2->get = " << any_ptr2.cast<std::shared_ptr<Data>>()->get() << endl;

        cout << "ptr.get = " << ptr.get() << endl;
        cout << "ptr usecount = " << ptr.use_count() << endl;
        SOLID_CHECK(ptr.use_count() == 3);
        SOLID_CHECK(ptr.get() == any_ptr1.cast<std::shared_ptr<Data>>()->get() and any_ptr2.cast<std::shared_ptr<Data>>()->get() == ptr.get());

        Any<256> any_ptr3 = make_any<256, std::shared_ptr<Data>>(std::move(ptr));

        SOLID_CHECK(any_ptr2.cast<std::shared_ptr<Data>>()->get() == any_ptr2.cast<std::shared_ptr<Data>>()->get());
        SOLID_CHECK(any_ptr2.cast<std::shared_ptr<Data>>()->use_count() == 3);
        SOLID_CHECK(ptr.use_count() == 0);
    }
    {
        Any<any_data_size<Test>()> any_t{Test{10}};
        Any<>                      any_0{std::move(any_t)};

        SOLID_CHECK(any_t.empty());
        SOLID_CHECK(not any_0.empty());

        any_t = std::move(any_0);

        SOLID_CHECK(any_0.empty());
        SOLID_CHECK(not any_t.empty());

        any_0 = Test{5};
        SOLID_CHECK(not any_0.empty());
    }
    return 0;
}
