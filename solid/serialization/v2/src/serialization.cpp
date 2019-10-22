// solid/serialization/v2/src/serialization.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v2/serialization.hpp"
#include <atomic>

namespace solid {
namespace serialization {
namespace v2 {
//========================================================================
/*virtual*/ Base::~Base()
{
}
//========================================================================
namespace {

class ErrorCategory : public ErrorCategoryT {
public:
    enum {
        NoTypeE = 1,
        NoCastE,
    };
    ErrorCategory() {}

private:
    const char* name() const noexcept override
    {
        return "solid::serialization::TypeIdMap";
    }

    std::string message(int _ev) const override
    {
        switch (_ev) {
        case NoTypeE:
            return "Type not registered";
        case NoCastE:
            return "Cast not registered";
        default:
            return "Unknown";
        }
    }
};

const ErrorCategory ec;
} //namespace

/*static*/ ErrorConditionT TypeMapBase::error_no_cast()
{
    return ErrorConditionT(ErrorCategory::NoCastE, ec);
}
/*static*/ ErrorConditionT TypeMapBase::error_no_type()
{
    return ErrorConditionT(ErrorCategory::NoTypeE, ec);
}
/*virtual*/ TypeMapBase::~TypeMapBase() {}
//========================================================================
//========================================================================
namespace binary {
/*static*/ size_t Base::newTypeId()
{
    static std::atomic<size_t> crt_idx{0};
    return crt_idx.fetch_add(1);
}

void Base::doAddVersion(const size_t _type_idx, const uint32_t _version)
{
    if (_type_idx < version_offset_) {
        if (version_offset_ != InvalidIndex()) {
            version_vec_.insert(version_vec_.begin(), version_offset_ - _type_idx, InvalidIndex());
        }
        version_offset_ = _type_idx;
    }

    const size_t idx = _type_idx - version_offset_;
    if (idx < version_vec_.size()) {
    } else {
        version_vec_.resize(idx + 1, InvalidIndex());
    }
    version_vec_[idx] = _version;
}

//========================================================================
//========================================================================

} //namespace binary
} // namespace v2
} // namespace serialization
} // namespace solid
