#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/intrusiveptr.hpp"
#include <iostream>

using namespace solid;
using namespace std;

namespace {
class Test : public IntrusiveThreadSafeBase {
    string s_;
    int    i_ = 0;

public:
    Test(const string_view _s, const int _i)
        : s_(_s)
        , i_(_i)
    {
    }
};

struct TestFirst : Test {
    size_t sz_ = 0;

    TestFirst(const size_t _sz, const string_view _s, const int _i)
        : Test(_s, _i)
        , sz_(_sz)
    {
    }
};

struct TestWithCounter {
    string s_;
    int    i_ = 0;

    TestWithCounter(const string_view _s, const int _i)
        : s_(_s)
        , i_(_i)
    {
    }

    virtual ~TestWithCounter() {}

private:
    friend class TestIntrusivePolicy;
    mutable atomic<size_t> use_count_{0};
};

template <typename T>
concept TestIntrusiveC = std::is_base_of_v<TestWithCounter, T>;

struct TestIntrusivePolicy {
    static void acquire(const TestWithCounter& _p) noexcept
    {
        ++_p.use_count_;
    }
    static bool release(const TestWithCounter& _p) noexcept
    {
        return _p.use_count_.fetch_sub(1) == 1;
    }
    static size_t useCount(const TestWithCounter& _p) noexcept
    {
        return _p.use_count_.load(std::memory_order_relaxed);
    }

    template <class T>
    static void destroy(const T& _r)
    {
        delete &_r;
    }

    template <class T, class... Args>
    static T* create(Args&&... _args)
    {
        return new T(std::forward<Args>(_args)...);
    }
};

} // namespace

template <TestIntrusiveC T>
struct solid::intrusive_policy_dispatch<T> {
    using type = TestIntrusivePolicy;
};

int test_intrusiveptr(int argc, char* argvp[])
{
    Test t{"ceva", 10};
    auto test_ptr = make_intrusive<Test>("ceva", 10);
    auto twc_ptr  = make_intrusive<TestWithCounter>("ceva2", 11);

    cout << "sizeof(test_ptr): " << sizeof(test_ptr) << endl;
    cout << "sizeof(twc_ptr): " << sizeof(twc_ptr) << endl;

    {
        IntrusivePtr<Test> ptr;

        ptr = test_ptr;
        solid_check(ptr.useCount() == 2);
    }
    solid_check(test_ptr.useCount() == 1);
    {
        IntrusivePtr<Test> ptr{test_ptr};

        solid_check(ptr.useCount() == 2);
    }
    solid_check(test_ptr.useCount() == 1);
    {
        IntrusivePtr<Test> ptr{std::move(test_ptr)};

        solid_check(ptr.useCount() == 1);
        test_ptr = std::move(ptr);
    }
    solid_check(test_ptr.useCount() == 1);
    {
        IntrusivePtr<Test> ptr;
        ptr = std::move(test_ptr);

        solid_check(ptr.useCount() == 1);
        test_ptr = std::move(ptr);
    }
    solid_check(test_ptr.useCount() == 1);

    auto first_ptr = make_intrusive<TestFirst>(111UL, "ceva", 10);

    {
        IntrusivePtr<Test> ptr{static_pointer_cast<Test>(first_ptr)};

        solid_check(ptr && ptr.useCount() == 2);

        // auto base_ptr = static_pointer_cast<IntrusiveThreadSafeBase>(ptr);//must not compile
    }

    return 0;
}