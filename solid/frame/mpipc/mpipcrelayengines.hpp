// solid/frame/mpipc/mpipcrelayengines.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mpipc/mpipcrelayengine.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {
namespace mpipc {
namespace relay {

class SingleNameEngine: public EngineCore{
public:
    SingleNameEngine(Manager& _rm);
    ~SingleNameEngine();
    void connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname);
private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace relay
} //namespace mpipc
} //namespace frame
} //namespace solid

