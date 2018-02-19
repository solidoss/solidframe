#include "solid/serialization/v2/error.hpp"
#include <sstream>

namespace solid{
namespace serialization{
namespace v2{

namespace {

enum {
    Error_E = 1,
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
    case Error_E:
        oss << "Generic";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} //namespace

/*extern*/ const ErrorConditionT error_(Error_E, category);
    
}//namespace v2
}//namespace serialization
}//namespace solid
