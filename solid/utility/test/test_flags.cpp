#include "solid/system/exception.hpp"
#include "solid/utility/flags.hpp"

using namespace solid;

enum struct Fruits {
    Apple,
    Banana,
    Cherry,
    Berry = 10,
    Last
};

using FruitsFlagsT = solid::Flags<Fruits, Fruits::Last>;

FruitsFlagsT add_banana(const FruitsFlagsT& _fruits)
{
    return _fruits | Fruits::Banana;
}

bool check(const FruitsFlagsT& _fruits)
{
    return _fruits.has(Fruits::Banana);
}

int test_flags(int argc, char* argv[])
{
    {
        FruitsFlagsT fruits;
        fruits.set(Fruits::Apple).set(Fruits::Berry);
        SOLID_CHECK(fruits.has(Fruits::Apple));
        SOLID_CHECK(fruits.has(Fruits::Berry));
        SOLID_CHECK(!fruits.has(Fruits::Cherry));
        SOLID_CHECK(!fruits.has(Fruits::Banana));
    }
    {
        FruitsFlagsT fruits{Fruits::Apple, Fruits::Berry};
        SOLID_CHECK(fruits.has(Fruits::Apple));
        SOLID_CHECK(fruits.has(Fruits::Berry));
        SOLID_CHECK(!fruits.has(Fruits::Cherry));
        SOLID_CHECK(!fruits.has(Fruits::Banana));

        fruits = add_banana(fruits);

        SOLID_CHECK(fruits.has(Fruits::Apple));
        SOLID_CHECK(fruits.has(Fruits::Berry));
        SOLID_CHECK(!fruits.has(Fruits::Cherry));
        SOLID_CHECK(fruits.has(Fruits::Banana));
    }
    return 0;
}
