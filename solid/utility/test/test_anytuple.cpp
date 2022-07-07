#include <iostream>
#include <string>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/utility/any.hpp"

using namespace std;
using namespace solid;

struct Alpha {
    uint64_t id_;
    string   name_;
};

struct Beta {
    vector<int> id_vec_;
};

int test_anytuple(int /*argc*/, char* /*argv*/[])
{
    // using TupleT = std::tuple<Alpha, Beta, string, vector<string>>;

    Any<> anytuple{make_tuple(Alpha{}, Beta{}, string("something"), vector<string>{"one", "two", "three"})};

    solid_check(anytuple.is_tuple());
    solid_check(anytuple.get_if<string>() != nullptr && *anytuple.get_if<string>() == "something");

    cout << "tuple<string>: " << *anytuple.get_if<string>() << endl;

    solid_check(anytuple.get_if<vector<string>>() && anytuple.get_if<vector<string>>()->size() == 3);

    cout << "tuple<vector>[0]: " << anytuple.get_if<vector<string>>()->at(0) << endl;

    solid_check(anytuple.get_if<Beta>() != nullptr);

    anytuple.get_if<Beta>()->id_vec_ = {1, 2, 3};
    solid_check(anytuple.get_if<Beta>()->id_vec_.size() == 3);

    Any<> anytuple2{std::move(anytuple)};

    solid_check(!anytuple);
    solid_check(anytuple2);

    solid_check(anytuple2.get_if<Beta>() != nullptr);
    solid_check(anytuple2.get_if<Beta>()->id_vec_.size() == 3);

    Any<> anytuple3{anytuple2};

    solid_check(anytuple2);
    solid_check(anytuple3);

    solid_check(anytuple3.get_if<Beta>() != nullptr);
    solid_check(anytuple3.get_if<Beta>()->id_vec_.size() == 3);
    solid_check(anytuple3.get_if<string>() != nullptr && *anytuple3.get_if<string>() == "something");
    return 0;
}
