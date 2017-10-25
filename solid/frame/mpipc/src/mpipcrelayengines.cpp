// solid/frame/ipc/src/mpipcrelayengines.cpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mpipc/mpipcrelayengines.hpp"
#include "mpipcutility.hpp"
#include "solid/system/debug.hpp"
#include "solid/utility/string.hpp"
#include <deque>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>

using namespace std;

namespace solid {
namespace frame {
namespace mpipc {
namespace relay {
//-----------------------------------------------------------------------------
namespace{
using ConnectionMapT   = std::unordered_map<const char*, size_t, CStringHash, CStringEqual>;
} //namespace

struct EngineCore::Data {
    ConnectionMapT   con_umap_;
};
//-----------------------------------------------------------------------------
SingleNameEngine::SingleNameEngine(Manager& _rm):EngineCore(_rm), impl_(make_pimpl<Data>()){}
//-----------------------------------------------------------------------------
SingleNameEngine::~SingleNameEngine(){
}
//-----------------------------------------------------------------------------
void SingleNameEngine::connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname){
    to_lower(_uname);
}
//-----------------------------------------------------------------------------
} //namespace relay
} //namespace mpipc
} //namespace frame
} //namespace solid
