#include "solid/utility/function.hpp"
#include <fstream>
#include <iostream>
using namespace solid;
using namespace std;

void test_fnc(void* _ptr, const char* _txt)
{
    cout << "test_fnc: " << _ptr << " " << _txt << endl;
}

class Test2 {
    Function<void(void*, const char*), 64> fnc_;

public:
    template <class F>
    void set(F _f)
    {
        auto lambda = [_f](void* _p, const char* _t) mutable {
            _f(_p, _t);
        };
        fnc_ = std::move(lambda);
    }

    void call(void* _p, const char* _t)
    {
        fnc_(_p, _t);
    }
};

class Test3 {
    std::function<void(void*, const char*)> fnc_;

public:
    template <class F>
    void set(F _f)
    {
        auto lambda = [_f](void* _p, const char* _t) mutable {
            _f(_p, _t);
        };
        fnc_ = std::move(lambda);
    }

    void call(void* _p, const char* _t)
    {
        fnc_(_p, _t);
    }
};

int test_function(int /*argc*/, char* /*argv*/ [])
{
    {
        Function<void(void*, const char*), 64> fnc(&test_fnc);
        fnc(&fnc, "something");
    }
    {
        ifstream                               ifs;
        Function<void(void*, const char*), 64> fnc(
            [ifs = std::move(ifs)](void* _ptr, const char* _txt) mutable {
                ifs.open("test.txt");
                cout << "test_fnc: " << _ptr << " " << _txt << endl;
            });
        fnc(&fnc, "something");
    }
#if 0
    {
        ifstream ifs;
        auto lambda = [ifs = std::move(ifs)](void *_ptr, const char*_txt)mutable{
            ifs.open("test.txt");
            cout<<"test_fnc: "<<_ptr<<" "<<_txt<<endl;
        };
        std::function<void(void*, const char*)> fnc(std::move(lambda));
        
        fnc(&fnc, "something");
    }
#endif

#if 1
    {
        std::string s{"sdfadsfads"};
        Test2       t;
        auto        lambda = [s = std::move(s)](void* _p, const char* _t) {
            cout << s << " " << _p << " " << _t << endl;
        };
        t.set(lambda);
        t.call(&t, "ceva");
    }
#endif
    {
        std::function<void(void*, const char*)> fnc(&test_fnc);
        fnc(&fnc, "something");
    }
#if 1
    {
        std::string s{"sdfadsfads"};
        Test3       t;
        auto        lambda = [s = std::move(s)](void* _p, const char* _t) {
            cout << s << " " << _p << " " << _t << endl;
        };
        t.set(lambda);
        t.call(&t, "ceva");
    }
#endif
    return 0;
}
