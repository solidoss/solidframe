// solid/system/src/socketaddress.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/socketaddress.hpp"

namespace solid {

#ifdef SOLID_HAS_NO_INLINES
#include "solid/system/socketaddress.ipp"
#endif

//-----------------------------------------------------------------------
//          synchronous_resolve
//-----------------------------------------------------------------------
namespace {

class ResolveErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "solid::resolve";
    }
    [[nodiscard]] std::string message(int _ev) const override
    {
#ifdef SOLID_ON_WINDOWS
        return gai_strerrorA(_ev);
#else
        return gai_strerror(_ev);
#endif
    }
};

const ResolveErrorCategory resolve_category_instance;

std::error_code make_resolve_error(const int _rv)
{
#ifdef EAI_SYSTEM
    if (_rv == EAI_SYSTEM) {
        return last_system_error();
    }
#endif
    return std::error_code(_rv, resolve_category());
}

std::expected<ResolveData, std::error_code> do_resolve(std::string_view _node, std::string_view _service, const addrinfo* _phints)
{
    if (_node.empty() || _service.empty()) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    const std::string node{_node};
    const std::string service{_service};
    addrinfo*         paddr = nullptr;
    const int         rv    = getaddrinfo(node.c_str(), service.c_str(), _phints, &paddr);
    if (rv != 0) {
        return std::unexpected(make_resolve_error(rv));
    }
    return ResolveData(paddr);
}

} // namespace

const std::error_category& resolve_category() noexcept
{
    return resolve_category_instance;
}

void ResolveData::delete_addrinfo(void* _pv)
{
    if (_pv != nullptr) {
        freeaddrinfo(reinterpret_cast<addrinfo*>(_pv));
    }
}

std::expected<ResolveData, std::error_code> synchronous_resolve(std::string_view _node, std::string_view _service)
{
    return do_resolve(_node, _service, nullptr);
}

std::expected<ResolveData, std::error_code> synchronous_resolve(
    std::string_view _node,
    std::string_view _service,
    int              _flags,
    int              _family,
    int              _type,
    int              _proto)
{
    addrinfo h{};
    h.ai_flags    = _flags;
    h.ai_family   = _family < 0 ? AF_UNSPEC : _family;
    h.ai_socktype = _type < 0 ? 0 : _type;
    h.ai_protocol = _proto < 0 ? 0 : _proto;
    return do_resolve(_node, _service, &h);
}

std::expected<ResolveData, std::error_code> synchronous_resolve(std::string_view _node, int _port)
{
    return synchronous_resolve(_node, std::to_string(_port));
}

std::expected<ResolveData, std::error_code> synchronous_resolve(
    std::string_view _node,
    int              _port,
    int              _flags,
    int              _family,
    int              _type,
    int              _proto)
{
    return synchronous_resolve(_node, std::to_string(_port), _flags, _family, _type, _proto);
}

std::expected<ReverseResolveData, std::error_code> synchronous_resolve(const SocketAddressStub& _rsa, int _flags)
{
    char      hbuf[NI_MAXHOST];
    char      sbuf[NI_MAXSERV];
    const int rv = getnameinfo(_rsa.sockAddr(), _rsa.size(), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), _flags);
    if (rv != 0) {
        return std::unexpected(make_resolve_error(rv));
    }
    return ReverseResolveData{.host = hbuf, .service = sbuf};
}

namespace {
template <class SA>
std::ostream& print_numeric_host(std::ostream& _ros, const SA& _rsa)
{
    if (const auto rv = synchronous_resolve(_rsa, ReverseResolveInfo::NumericHost /* | ReverseResolveInfo::NumericService*/)) {
        _ros << rv->host;
    }
    return _ros;
}
} // namespace

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet4& _rsa)
{
    return print_numeric_host(_ros, _rsa);
}

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet& _rsa)
{
    return print_numeric_host(_ros, _rsa);
}

std::ostream& operator<<(std::ostream& _ros, const SocketAddress& _rsa)
{
    return print_numeric_host(_ros, _rsa);
}

} // namespace solid
