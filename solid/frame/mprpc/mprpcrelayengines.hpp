// solid/frame/mprpc/mprpcrelayengines.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mprpc/mprpcrelayengine.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {
namespace mprpc {
namespace relay {

class SingleNameEngine : public EngineCore {
public:
    SingleNameEngine(Manager& _rm);
    ~SingleNameEngine();
    ErrorConditionT registerConnection(const ConnectionContext& _rconctx, std::string&& _uname);

private:
    void          unregisterConnectionName(Proxy& _proxy, size_t _conidx) override;
    size_t        registerConnection(Proxy& _proxy, std::string&& _uname) override;
    std::ostream& print(std::ostream& _ros, const ConnectionStubBase& _rcon) const override;

private:
    struct Data;
    Pimpl<Data, 96> impl_;
};

} // namespace relay
} // namespace mprpc
} // namespace frame
} // namespace solid
