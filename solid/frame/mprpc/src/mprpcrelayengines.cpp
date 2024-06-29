// solid/frame/ipc/src/mprpcrelayengines.cpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mprpc/mprpcrelayengines.hpp"
#include "mprpcutility.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/string.hpp"
#include <deque>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>

using namespace std;

namespace solid {
namespace {
const LoggerT logger("solid::frame::mprpc::relay");
}

namespace frame {
namespace mprpc {
namespace relay {
//-----------------------------------------------------------------------------
namespace {
using ConnectionMapT = std::unordered_map<uint32_t, size_t>;
} // namespace

struct SingleNameEngine::Data {
    ConnectionMapT con_umap_;
};
//-----------------------------------------------------------------------------
SingleNameEngine::SingleNameEngine(Manager& _rm)
    : EngineCore(_rm)
{
}
//-----------------------------------------------------------------------------
SingleNameEngine::~SingleNameEngine()
{
}
//-----------------------------------------------------------------------------
ErrorConditionT SingleNameEngine::registerConnection(const ConnectionContext& _rconctx, const uint32_t _group_id, const uint16_t _replica_id)
{
    ErrorConditionT err;
    auto            lambda = [_group_id, _replica_id, this, &_rconctx /*, &err*/](EngineCore::Proxy& _proxy) {
        size_t conidx = static_cast<size_t>(_rconctx.relayId().index);
        size_t idx    = InvalidIndex();

        {
            const auto it = impl_->con_umap_.find(_group_id);

            if (it != impl_->con_umap_.end()) {
                idx = it->second;
            }
        }

        if (conidx == InvalidIndex()) {
            if (idx == InvalidIndex()) {
                // do full registration
                conidx                           = _proxy.createConnection();
                ConnectionStubBase& rcon         = _proxy.connection(conidx);
                rcon.group_id_                   = _group_id;
                rcon.replica_id_                 = _replica_id;
                impl_->con_umap_[rcon.group_id_] = conidx;
            } else {
                if (_proxy.connection(idx).id_.isInvalid() || _proxy.connection(idx).id_ == _rconctx.connectionId()) {
                    // use the connection already registered by name
                    conidx = idx;
                } else {
                    // for now the most basic option - replace the existing connection with new one
                    // TODO: add support for multiple chained connections, sharing the same name
                    impl_->con_umap_.erase(_proxy.connection(idx).group_id_);
                    _proxy.connection(idx).group_id_ = InvalidIndex();
                    conidx                           = _proxy.createConnection();
                    ConnectionStubBase& rcon         = _proxy.connection(conidx);
                    rcon.group_id_                   = _group_id;
                    rcon.replica_id_                 = _replica_id;
                    impl_->con_umap_[rcon.group_id_] = conidx;
                }
            }
        } else if (idx != InvalidIndex()) {
            // conflicting situation
            //  - the connection was used for sending relayed messages - thus was registered without a name
            //  - also the name was associated to another connection stub
            _proxy.stopConnection(conidx);
            conidx = idx;
        } else {
            // simply register the name for existing connection
            ConnectionStubBase& rcon         = _proxy.connection(conidx);
            rcon.group_id_                   = _group_id;
            rcon.replica_id_                 = _replica_id;
            impl_->con_umap_[rcon.group_id_] = conidx;
        }

        ConnectionStubBase& rcon = _proxy.connection(conidx);
        rcon.id_                 = _rconctx.connectionId();
        _proxy.registerConnectionId(_rconctx, conidx);

        solid_check_log(_proxy.notifyConnection(_proxy.connection(conidx).id_, RelayEngineNotification::NewData), logger, "Connection should be alive");
    };

    execute(lambda);

    return err;
}
//-----------------------------------------------------------------------------
void SingleNameEngine::unregisterConnectionName(Proxy& _proxy, size_t _conidx) /*override*/
{
    impl_->con_umap_.erase(_proxy.connection(_conidx).group_id_);
}
//-----------------------------------------------------------------------------
size_t SingleNameEngine::registerConnection(Proxy& _proxy, const uint32_t _group_id, const uint16_t _replica_id) /*override*/
{
    size_t     conidx = InvalidIndex();
    const auto it     = impl_->con_umap_.find(_group_id);

    if (it != impl_->con_umap_.end()) {
        conidx = it->second;
    } else {
        conidx                           = _proxy.createConnection();
        ConnectionStubBase& rcon         = _proxy.connection(conidx);
        rcon.group_id_                   = _group_id;
        rcon.replica_id_                 = _replica_id;
        impl_->con_umap_[rcon.group_id_] = conidx;
    }
    return conidx;
}
//-----------------------------------------------------------------------------
std::ostream& SingleNameEngine::print(std::ostream& _ros, const ConnectionStubBase& _rcon) const /*override*/
{
    return _ros << "con.id = " << _rcon.id_ << " con = " << _rcon.group_id_ << ", " << _rcon.replica_id_;
}
//-----------------------------------------------------------------------------
} // namespace relay
} // namespace mprpc
} // namespace frame
} // namespace solid
