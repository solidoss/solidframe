#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <array>
#include <expected>
#include <iostream>
#include <span>
#include <string_view>

using namespace std;
using namespace solid;

namespace {
span<const byte> as_bytes(string_view _s)
{
    return std::as_bytes(span<const char>{_s.data(), _s.size()});
}

template <class T1, class T2, bool opt>
auto get_if(T1&& t1, T2&& t2, bool _b)
{
    if constexpr (opt) {
        return _b ? std::expected<T1, std::error_code>{t1} : std::unexpected(last_system_error());
    } else {
        return _b ? std::expected<T2, std::error_code>{t2} : std::unexpected(last_system_error());
    }
}
} // namespace

int test_socketdevice(int /*argc*/, char* /*argv*/[])
{
    const string_view msg = "ping";
    array<char, 64>   buf{};

    // resolve
    {
        const auto rd = synchronous_resolve("127.0.0.1", 80, DirectResoveInfo::Numeric, SocketInfo::Inet4, SocketInfo::Stream);
        solid_check(rd && !rd->empty());
        const SocketAddressInet4 sa(rd->begin());
        solid_check(sa.port() == 80);

        const auto rrv = synchronous_resolve(sa, ReverseResolveInfo::Numeric);
        solid_check(rrv && rrv->host == "127.0.0.1" && rrv->service == "80");

        const auto bad = synchronous_resolve("", "80");
        solid_check(!bad && bad.error() == errc::invalid_argument);

        const auto bad2 = synchronous_resolve("999.999.999.999", 80, DirectResoveInfo::Numeric);
        solid_check(!bad2 && bad2.error().category() == resolve_category());
        solid_check(!bad2.error().message().empty());
    }

    // listener
    SocketDevice listener;
    solid_check(listener.create(SocketInfo::Inet4, SocketInfo::Stream));
    solid_check(listener.prepareAccept(SocketAddressInet4("127.0.0.1", 0), 10));
    const auto local = listener.localAddress();
    solid_check(local && local->isInet4());
    const int port = local->port();
    solid_check(port > 0);

    // client
    SocketDevice client;
    solid_check(client.create(SocketInfo::Inet4, SocketInfo::Stream));
    solid_check(client.makeNonBlocking());
    solid_check(client.isBlocking().value() == false);
    {
        const auto rv = client.connect(SocketAddressInet4("127.0.0.1", port));
        solid_check(rv || SocketDevice::can_wait_connect(rv.error()));
    }

    auto accepted = listener.accept();
    solid_check(accepted);
    SocketDevice server = std::move(*accepted);
    solid_check(server);

    solid_check(client.makeBlocking(1000));
    solid_check(client.isBlocking().value() == true);
    solid_check(!client.error());

    // options
    solid_check(client.enableNoDelay());
    solid_check(client.hasNoDelay().value() == true);
    solid_check(client.disableNoDelay());
    solid_check(client.hasNoDelay().value() == false);
    solid_check(client.sendBufferSize(64 * 1024));
    solid_check(client.sendBufferSize().value() >= 64 * 1024);
    solid_check(client.recvBufferSize().value() > 0);
    solid_check(client.type().value() == SocketInfo::Stream);

    // stream send/recv
    solid_check(client.send(as_bytes(msg)).value() == msg.size());
    {
        const auto rv = server.recv(as_writable_bytes(span{buf}));
        solid_check(rv && *rv == msg.size() && string_view(buf.data(), *rv) == msg);
    }

    // addresses
    solid_check(server.remoteAddress().value().port() == client.localAddress().value().port());
    solid_check(client.remoteAddress().value().port() == port);

    // non-blocking recv with no data -> can retry
    solid_check(server.makeNonBlocking());
    {
        const auto rv = server.recv(as_writable_bytes(span{buf}));
        solid_check(!rv && SocketDevice::can_retry_io(rv.error()));
        solid_check(!SocketDevice::can_retry_accept(std::error_code{}));
    }
    // non-blocking accept with no pending connection -> can retry
    solid_check(listener.makeNonBlocking());
    {
        const auto rv = listener.accept();
        solid_check(!rv && SocketDevice::can_retry_accept(rv.error()));
    }
    // orderly shutdown -> recv returns 0
    solid_check(auto const ret_val{client.shutdownWrite()}, "unexpected " << ret_val.error());

    solid_check(auto const ret_val{(get_if<std::string, std::uint64_t, false>("class", 10, true))}, "unexpected " << ret_val.error().message());
    solid_check(server.makeBlocking());
    solid_check(server.recv(as_writable_bytes(span{buf})).value() == 0);

    // datagram send/recv
    SocketDevice udp1;
    SocketDevice udp2;
    solid_check(udp1.create(SocketInfo::Inet4, SocketInfo::Datagram));
    solid_check(udp2.create(SocketInfo::Inet4, SocketInfo::Datagram));
    solid_check(udp1.bind(SocketAddressInet4("127.0.0.1", 0)));
    solid_check(udp2.bind(SocketAddressInet4("127.0.0.1", 0)));
    solid_check(udp2.send(as_bytes(msg), udp1.localAddress().value()).value() == msg.size());
    {
        SocketAddress from;
        const auto    rv = udp1.recv(as_writable_bytes(span{buf}), from);
        solid_check(rv && *rv == msg.size() && string_view(buf.data(), *rv) == msg);
        solid_check(from.isInet4() && from.port() == udp2.localAddress().value().port());
    }

    // operations on a closed socket fail with a system error
    client.close();
    solid_check(!client);
    {
        const auto rv = client.send(as_bytes(msg));
        solid_check(!rv && rv.error() == errc::bad_file_descriptor);
    }

    cout << "done" << endl;
    return 0;
}
