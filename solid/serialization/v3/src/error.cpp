// solid/serialization/v3/src/error.cpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include <sstream>

#include "solid/serialization/v3/error.hpp"

namespace solid {
namespace serialization {
inline namespace v3 {

namespace {

enum {
    Error_Limit_Container_E = 1,
    Error_Limit_String_E,
    Error_Limit_Stream_E,
    Error_Limit_Blob_E,
    Error_Limit_Array_E,
    Error_Compacted_Integer_E,
    Error_Unknown_Type_E,
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}
    const char* name() const noexcept override
    {
        return "solid::serialization";
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
    case Error_Limit_Container_E:
        oss << "Limit container";
        break;
    case Error_Limit_String_E:
        oss << "Limit string";
        break;
    case Error_Limit_Stream_E:
        oss << "Limit stream";
        break;
    case Error_Limit_Blob_E:
        oss << "Limit blob";
        break;
    case Error_Limit_Array_E:
        oss << "Limit array";
        break;
    case Error_Compacted_Integer_E:
        oss << "Compacted integer checks failed";
        break;
    case Error_Unknown_Type_E:
        oss << "Unknown type. Type not available in typemap";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}

} // namespace

/*extern*/ const ErrorConditionT error_limit_container(Error_Limit_Container_E, category);
/*extern*/ const ErrorConditionT error_limit_string(Error_Limit_String_E, category);
/*extern*/ const ErrorConditionT error_limit_stream(Error_Limit_Stream_E, category);
/*extern*/ const ErrorConditionT error_limit_blob(Error_Limit_Blob_E, category);
/*extern*/ const ErrorConditionT error_compacted_integer(Error_Compacted_Integer_E, category);
/*extern*/ const ErrorConditionT error_unknown_type(Error_Unknown_Type_E, category);
/*extern*/ const ErrorConditionT error_limit_array(Error_Limit_Blob_E, category);
} // namespace v3
} // namespace serialization
} // namespace solid
