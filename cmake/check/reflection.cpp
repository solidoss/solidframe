#include <meta>
#include <string_view>

struct Test {
    int    a;
    double b;
};

int main()
{
    constexpr auto ctx = std::meta::access_context::unchecked();
    static_assert(std::meta::nonstatic_data_members_of(^^Test, ctx).size() == 2);
    constexpr std::string_view name = std::meta::identifier_of(^^Test::a);
    static_assert(name == "a");
    return 0;
}
