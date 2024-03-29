#include "test_pimpl.hpp"
#include <iostream>
using namespace std;

int test_pimpl(int argc, char* argv[])
{
    Test test1;
    Test test2{1, "something"};

    cout << "_1 ";
    test1.print(cout);
    cout << "_2 ";
    test2.print(cout);
    // test2.value() = "test";
    test2.print(cout);
#if 1
    {
        Test test3{test2};
        cout << "_3 ";
        test3.print(cout);
    }
    {
        Test test4{std::move(test2)};
        cout << "_4 ";
        test4.print(cout);
        cout << "_2 ";
        test2.print(cout);
    }
#endif

    {
        TestMoveOnly testmo1{2, "somethingelse"};
        TestMoveOnly testmo2{std::move(testmo1)};
        testmo2.print(cout);
    }
    return 0;
}