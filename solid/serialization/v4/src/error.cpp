// solid/serialization/v4/src/error.cpp
//
// Copyright (c) 2026 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include <string>

#include "solid/serialization/v4/error.hpp"

namespace solid::serialization::v4 {

namespace {

enum {
    Error_Limit_Container_E = 1,
    Error_Limit_String_E,
    Error_Limit_Stream_E,
    Error_Limit_Array_E,
    Error_Compacted_Integer_E,
    Error_Unknown_Type_E,
    Error_Variant_Index_E,
    Error_Reflector_Type_E,
};

class ErrorCategory : public ErrorCategoryT {
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "solid::serialization::v4";
    }
    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::string msg = "(";
    msg += name();
    msg += ':';
    msg += std::to_string(_ev);
    msg += "): ";

    switch (_ev) {
    case 0:
        msg += "Success";
        break;
    case Error_Limit_Container_E:
        msg += "Limit container";
        break;
    case Error_Limit_String_E:
        msg += "Limit string";
        break;
    case Error_Limit_Stream_E:
        msg += "Limit stream";
        break;
    case Error_Limit_Array_E:
        msg += "Limit array";
        break;
    case Error_Compacted_Integer_E:
        msg += "Compacted integer checks failed";
        break;
    case Error_Unknown_Type_E:
        msg += "Unknown type. Type not available in typemap";
        break;
    case Error_Variant_Index_E:
        msg += "Variant index out of range";
        break;
    case Error_Reflector_Type_E:
        msg += "Pointer element type not registered as a base of the type in typemap";
        break;
    default:
        msg += "Unknown";
        break;
    }
    return msg;
}

} // namespace

/*extern*/ const ErrorConditionT error_limit_container(Error_Limit_Container_E, category);
/*extern*/ const ErrorConditionT error_limit_string(Error_Limit_String_E, category);
/*extern*/ const ErrorConditionT error_limit_stream(Error_Limit_Stream_E, category);
/*extern*/ const ErrorConditionT error_limit_array(Error_Limit_Array_E, category);
/*extern*/ const ErrorConditionT error_compacted_integer(Error_Compacted_Integer_E, category);
/*extern*/ const ErrorConditionT error_unknown_type(Error_Unknown_Type_E, category);
/*extern*/ const ErrorConditionT error_variant_index(Error_Variant_Index_E, category);
/*extern*/ const ErrorConditionT error_reflector_type(Error_Reflector_Type_E, category);

} // namespace solid::serialization::v4
