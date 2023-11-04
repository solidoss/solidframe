#pragma once
#include "solid/system/pimpl.hpp"
#include <ostream>
#include <string_view>

class Test {
    struct Data;
    solid::Pimpl<Data, 64> pimpl_;

public:
    Test();
    Test(int _v1, const std::string_view& _v2);
    ~Test();

    Test(const Test&);
    Test(Test&&);

    std::ostream& print(std::ostream& _ros) const;

    std::string const& value() const;
};

class TestMoveOnly {
    struct Data;
    solid::Pimpl<Data, 64> pimpl_;

public:
    TestMoveOnly();
    TestMoveOnly(int _v1, const std::string_view& _v2);
    ~TestMoveOnly();

    // TestMoveOnly(const TestMoveOnly&);
    TestMoveOnly(TestMoveOnly&&);

    std::ostream& print(std::ostream& _ros) const;
};
