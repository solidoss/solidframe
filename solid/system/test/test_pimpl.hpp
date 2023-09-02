#pragma once
#include "solid/system/pimpl.hpp"
#include <ostream>
#include <string_view>

class Test {
    struct Data;
    solid::Pimpl<Data, 40> pimpl_;

public:
    Test();
    Test(int _v1, const std::string_view& _v2);
    ~Test();

    Test(const Test&);
    Test(Test&&);

    std::ostream& print(std::ostream& _ros) const;
};
