#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"

#include <functional>
using namespace std;
using namespace solid;
namespace {
class Test {
    using FunctionT = Function<int(std::string&), 32>;
    // using FunctionT = std::function<int(std::string&)>;
    FunctionT f_;
    template <typename F>
    struct Functor {
        F   f_;
        int i_;

        Functor(F&& _f, int _i)
            : f_{std::forward<F>(_f)}
            , i_{_i}
        {
        }

        int operator()(std::string& _s)
        {
            return i_ + f_(_s);
        }
    };

public:
    template <typename F>
    void store(F&& _f)
    {
        f_ = _f;
    }

    template <typename F>
    void store(F&& _f, int _i)
    {
        doStore(std::forward<F>(_f), _i);
    }

    int operator()(std::string& _str)
    {
        FunctionT f{std::move(f_)};
        // f = f_;
        return f(_str);
    }

    static int static_function(std::string& _s)
    {
        return 1;
    }

    template <typename F>
    void doStore(F&& _f, int _i)
    {
        Functor<F> f{std::forward<F>(_f), _i};
        f_ = std::move(f);
    }
};

struct Functor {
    int i_ = 2;
    int operator()(std::string&)
    {
        return i_;
    }
};

} // namespace

int test_template_function(int argc, char* argv[])
{
    std::string s{"test"};
    {
        Test t;
        t.store(Test::static_function);
        solid_check(t(s) == 1);
    }
    {
        Test t;
        t.store(Functor{});
        solid_check(t(s) == 2);
    }
    {
        Test    t;
        Functor f;
        t.store(f);
        solid_check(t(s) == 2);
    }
    {
        Test    t;
        Functor f;
        f.i_ = 3;
        t.store(std::move(f));
        solid_check(t(s) == 3);
    }

    {
        Test t;
        t.store([](string&) { return 4; });
        solid_check(t(s) == 4);
    }
    {
        Test t;
        auto l = [](string&) { return 5; };
        t.store(l);
        solid_check(t(s) == 5);
    }
    {
        Test t;
        auto l = [](string&) { return 6; };
        t.store(std::move(l));
        solid_check(t(s) == 6);
    }

    // functor
    {
        Test t;
        t.store(Test::static_function, 1);
        solid_check(t(s) == 2);
    }
    {
        Test t;
        t.store(Functor{}, 1);
        solid_check(t(s) == 3);
    }
    {
        Test    t;
        Functor f;
        t.store(f, 1);
        solid_check(t(s) == 3);
    }
    {
        Test    t;
        Functor f;
        f.i_ = 3;
        t.store(std::move(f), 1);
        solid_check(t(s) == 4);
    }

    {
        Test t;
        t.store([](string&) { return 4; }, 1);
        solid_check(t(s) == 5);
    }
    {
        Test t;
        auto l = [](string&) { return 5; };
        t.store(l, 1);
        solid_check(t(s) == 6);
    }
    {
        Test t;
        auto l = [](string&) { return 6; };
        t.store(std::move(l), 1);
        solid_check(t(s) == 7);
    }

    return 0;
}
