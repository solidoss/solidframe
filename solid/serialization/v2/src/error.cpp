#include "solid/serialization/v2/error.hpp"
#include <sstream>

namespace solid {
namespace serialization {
namespace v2 {

namespace {

enum {
    Error_Limit_Container_E = 1,
    Error_Limit_String_E,
    Error_Limit_Stream_E,
    Error_Cross_Integer_E,
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept
    {
        return "solid::serialization";
    }
    std::string message(int _ev) const;
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
    case Error_Limit_Container_E:
        oss << "Limit container";
        break;
    case Error_Limit_String_E:
        oss << "Limit string";
        break;
    case Error_Limit_Stream_E:
        oss << "Limit stream";
        break;
    case Error_Cross_Integer_E:
        oss << "Cross integer checks failed";
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} //namespace

/*extern*/ const ErrorConditionT error_limit_container(Error_Limit_Container_E, category);
/*extern*/ const ErrorConditionT error_limit_string(Error_Limit_Container_E, category);
/*extern*/ const ErrorConditionT error_limit_stream(Error_Limit_Container_E, category);
/*extern*/ const ErrorConditionT error_cross_integer(Error_Cross_Integer_E, category);

} //namespace v2
} //namespace serialization
} //namespace solid
