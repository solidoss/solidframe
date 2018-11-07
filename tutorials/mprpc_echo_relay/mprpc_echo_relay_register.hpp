
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"

struct Register : solid::frame::mprpc::Message {
    std::string name;

    Register() {}

    Register(const std::string& _name)
        : name(_name)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.name, _rctx, "name");
    }
};

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

constexpr TypeIdT null_type_id{0, 0};
constexpr TypeIdT register_type_id{0, 1};
