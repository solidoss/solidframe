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
namespace {
using ConnectionMapT = std::unordered_map<const char*, size_t, CStringHash, CStringEqual>;
} //namespace

struct SingleNameEngine::Data {
    ConnectionMapT con_umap_;
};
//-----------------------------------------------------------------------------
SingleNameEngine::SingleNameEngine(Manager& _rm)
    : EngineCore(_rm)
    , impl_(make_pimpl<Data>())
{
}
//-----------------------------------------------------------------------------
SingleNameEngine::~SingleNameEngine()
{
}
//-----------------------------------------------------------------------------
ErrorConditionT SingleNameEngine::registerConnection(const ObjectIdT& _rconuid, std::string&& _uname)
{
    SOLID_ASSERT(not _uname.empty());
    ErrorConditionT err;
    auto lambda = [&_uname, this, &_rconuid, &err](EngineCore::Proxy& _proxy) {

        size_t conidx = _proxy.findConnection(_rconuid);

        size_t nameidx = InvalidIndex();

        {
            const auto it = impl_->con_umap_.find(_uname.c_str());

            if (it != impl_->con_umap_.end()) {
                nameidx = it->second;
            }
        }

        if (conidx == InvalidIndex()) {
            if (nameidx == InvalidIndex()) {
                //do full registration
                conidx                               = _proxy.createConnection();
                ConnectionStubBase& rcon             = _proxy.connection(conidx);
                rcon.name_                           = std::move(_uname);
                impl_->con_umap_[rcon.name_.c_str()] = conidx;
            } else {
                if (_proxy.connection(nameidx).id_.isInvalid() or _proxy.connection(nameidx).id_ == _rconuid) {
                    //use the connection already registered by name
                    conidx = nameidx;
                } else {
                    //for now the most basic option - replace the existing connection with new one
                    //TODO: add support for multiple chained connections, sharing the same name
                    impl_->con_umap_.erase(_proxy.connection(nameidx).name_.c_str());
                    _proxy.connection(nameidx).name_.clear();
                    conidx                               = _proxy.createConnection();
                    ConnectionStubBase& rcon             = _proxy.connection(conidx);
                    rcon.name_                           = std::move(_uname);
                    impl_->con_umap_[rcon.name_.c_str()] = conidx;
                }
            }
        } else if (nameidx != InvalidIndex()) {
            //conflicting situation
            // - the connection was used for sending relayed messages - thus was registered without a name
            // - also the name was associated to another connection stub
            _proxy.stopConnection(conidx);
            conidx = nameidx;
        } else {
            //simply register the name for existing connection
            ConnectionStubBase& rcon             = _proxy.connection(conidx);
            rcon.name_                           = std::move(_uname);
            impl_->con_umap_[rcon.name_.c_str()] = conidx;
        }

        ConnectionStubBase& rcon = _proxy.connection(conidx);
        rcon.id_                 = _rconuid;
        _proxy.registerConnectionId(conidx);

        SOLID_CHECK(_proxy.notifyConnection(rcon.id_, RelayEngineNotification::NewData), "Connection should be alive");
    };

    to_lower(_uname);

    execute(lambda);

    return err;
}
//-----------------------------------------------------------------------------
void SingleNameEngine::unregisterConnectionName(Proxy& _proxy, size_t _conidx) /*override*/
{
    impl_->con_umap_.erase(_proxy.connection(_conidx).name_.c_str());
}
//-----------------------------------------------------------------------------
size_t SingleNameEngine::registerConnection(Proxy& _proxy, std::string&& _uname) /*override*/
{
    size_t     conidx = InvalidIndex();
    const auto it     = impl_->con_umap_.find(_uname.c_str());

    if (it != impl_->con_umap_.end()) {
        conidx = it->second;
    } else {
        conidx                               = _proxy.createConnection();
        ConnectionStubBase& rcon             = _proxy.connection(conidx);
        rcon.name_                           = std::move(_uname);
        impl_->con_umap_[rcon.name_.c_str()] = conidx;
    }
    return conidx;
}
//-----------------------------------------------------------------------------
std::ostream& SingleNameEngine::print(std::ostream& _ros, const ConnectionStubBase& _rcon) const /*override*/
{
    return _ros << " con.id = " << _rcon.id_ << " con.name = " << _rcon.name_;
}
//-----------------------------------------------------------------------------
} //namespace relay
} //namespace mpipc
} //namespace frame
} //namespace solid
