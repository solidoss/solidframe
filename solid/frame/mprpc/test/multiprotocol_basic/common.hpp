#pragma once

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/system/common.hpp"

#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"

typedef solid::frame::Scheduler<solid::frame::aio::Reactor> AioSchedulerT;

using TypeIdT = std::pair<uint8_t, uint8_t>;

namespace std {
//inject a hash for TypeIdT
template <>
struct hash<TypeIdT> {
    typedef TypeIdT     argument_type;
    typedef std::size_t result_type;
    result_type         operator()(argument_type const& s) const noexcept
    {
        result_type const h1(std::hash<uint8_t>{}(s.first));
        result_type const h2(std::hash<uint8_t>{}(s.second));
        return h1 ^ (h2 << 1); // || use boost::hash_combine (see Discussion)
    }
};
} //namespace std

using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<TypeIdT>;
