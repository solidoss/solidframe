#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include <iostream>
#include <sstream>

using namespace std;
namespace {

class ErrorCategory : public solid::ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "test";
    }
    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case 1:
        oss << "Test";
        break;
    default:
        oss << "Unknown";
    };
    return oss.str();
}

const solid::ErrorConditionT error_test{1, category};
} //namespace

int test_exception(int argc, char* argv[])
{
    bool        is_ok = false;
    std::string check_str;
    std::string check_condition_str;

    {
        ostringstream oss;
        const int     line = 62;
        oss << '[' << __FILE__ << '(' << line << ")][" << SOLID_FUNCTION_NAME << "] (argc == 0) check failed: some error: " << argc << " " << argv[0] << " " << argv[1];
        check_str = oss.str();
    }
    {
        ostringstream oss;
        const int     line = 71;
        oss << '[' << __FILE__ << '(' << line << ")][" << SOLID_FUNCTION_NAME << "] error_test:" << error_test.message();
        check_condition_str = oss.str();
    }
    try {
        solid_check(argc == 0, "some error: " << argc << " " << argv[0] << " " << argv[1]);
    } catch (std::runtime_error& _rerr) {
        is_ok = true;
        cout<<check_str<<endl;
        cout << _rerr.what() << endl;
        solid_assert(check_str == _rerr.what());
    }

    try {
        solid_check_error(argc == 0, error_test);
    } catch (solid::RuntimeError& _rerr) {
        cout<<_rerr.what()<<endl;
        cout<<check_condition_str<<endl;
        solid_assert(check_condition_str == _rerr.what());
        solid_assert(_rerr.error() == error_test);
    }

    solid_assert(is_ok);

    return 0;
}
