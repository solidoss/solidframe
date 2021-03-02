#include <iostream>
#include <string>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/utility/anytuple.hpp"

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
    //using TupleT = std::tuple<Alpha, Beta, string, vector<string>>;

    AnyTuple anytuple{make_tuple(Alpha{}, Beta{}, string("something"), vector<string>{"one", "two", "three"})};

    solid_check(anytuple.getIf<string>() != nullptr && *anytuple.getIf<string>() == "something");

    cout << "tuple<string>: " << *anytuple.getIf<string>() << endl;

    solid_check(anytuple.getIf<vector<string>>() && anytuple.getIf<vector<string>>()->size() == 3);

    cout << "tuple<vector>[0]: " << anytuple.getIf<vector<string>>()->at(0) << endl;

    solid_check(anytuple.getIf<Beta>() != nullptr);

    anytuple.getIf<Beta>()->id_vec_ = {1, 2, 3};
    solid_check(anytuple.getIf<Beta>()->id_vec_.size() == 3);

    AnyTuple anytuple2{std::move(anytuple)};

    solid_check(!anytuple);
    solid_check(anytuple2);

    solid_check(anytuple2.getIf<Beta>() != nullptr);
    solid_check(anytuple2.getIf<Beta>()->id_vec_.size() == 3);

    AnyTuple anytuple3{anytuple2};

    solid_check(anytuple2);
    solid_check(anytuple3);

    solid_check(anytuple3.getIf<Beta>() != nullptr);
    solid_check(anytuple3.getIf<Beta>()->id_vec_.size() == 3);
    solid_check(anytuple3.getIf<string>() != nullptr && *anytuple3.getIf<string>() == "something");
    return 0;
}
