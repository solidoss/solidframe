// solid/serialization/v3/src/binarybasic.cpp
//
// Copyright (c) 2021 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v3/binarybasic.hpp"
#include "solid/serialization/v3/binarybase.hpp"
#include "solid/system/cassert.hpp"

namespace solid {
namespace serialization {
namespace v3 {

const LoggerT logger{"solid::serialization"};

const LoggerT& serialization_logger()
{
    return logger;
}

namespace binary {
} // namespace binary

} // namespace v3
} // namespace serialization
} // namespace solid
