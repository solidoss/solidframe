#include <expected>
#include <print>

using namespace std;

struct base {
    template <class Self>
    void f(this Self&& self)
    {
        print("{0}", typeid(Self).name());
    }
};

struct derived : base {};

int main()
{
    derived my_derived;
    my_derived.f();

    return 0;
}