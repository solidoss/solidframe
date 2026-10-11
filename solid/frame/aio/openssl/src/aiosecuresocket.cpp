// solid/frame/aio/openssl/src/aiocompletion.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/utility/common.hpp"

#ifdef SOLID_ON_WINDOWS
#define NOMINMAX
#include <WinSock2.h>
#include <windows.h>
#endif

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/aio/aioerror.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include <mutex>
#include <span>
#include <string>
#include <thread>

// #include <cstdio>

#include "openssl/bio.h"
#include "openssl/conf.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/ssl.h"

#ifdef SOLID_ON_WINDOWS
#pragma comment(lib, "crypt32")
#endif

namespace {

class OpenSSLErrorCategory : public solid::ErrorCategoryT {
public:
    OpenSSLErrorCategory() {}
    [[nodiscard]] const char* name() const noexcept override
    {
        return "OpenSSL";
    }
    [[nodiscard]] std::string message(int _ev) const override;

    [[nodiscard]] solid::ErrorCodeT makeError(unsigned long _err) const
    {
        return solid::ErrorCodeT(static_cast<int>(_err), *this);
    }
};

const OpenSSLErrorCategory ssl_category;

std::string OpenSSLErrorCategory::message(int _ev) const
{
    std::ostringstream oss;
    char               buf[1024];

    ERR_error_string_n(_ev, buf, 1024);

    oss << "(" << name() << ":" << _ev << "): " << buf;
    return oss.str();
}

enum struct WrapperError : int {
    Call,
    SetCheckHostName,
    SetCheckEmail,
    SetCheckIP,
};

class ErrorCategory : public solid::ErrorCategoryT {
public:
    ErrorCategory() {}
    [[nodiscard]] const char* name() const noexcept override
    {
        return "solid::frame::aio::openssl";
    }
    [[nodiscard]] std::string message(int _ev) const override;

    [[nodiscard]] solid::ErrorCodeT makeError(WrapperError _err) const
    {
        return solid::ErrorCodeT(static_cast<int>(_err), *this);
    }
};

const ErrorCategory wrapper_category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;
    oss << "(" << name() << ":" << _ev << "): ";

    switch (static_cast<WrapperError>(_ev)) {
    case WrapperError::Call:
        oss << "OpenSSL API call";
        break;
    case WrapperError::SetCheckHostName:
        oss << "Setting HostName used for verification";
        break;
    case WrapperError::SetCheckEmail:
        oss << "Setting Email used for verification";
        break;
    case WrapperError::SetCheckIP:
        oss << "Setting IP used for verification";
        break;
    default:
        oss << "Unknown error";
        break;
    }

    return oss.str();
}

} // namespace

namespace solid::frame::aio::openssl {

namespace {
const LoggerT logger("solid::frame::aio::openssl");
} // namespace

struct Starter {

    Starter()
    {
#ifndef OPENSSL_IS_BORINGSSL
        //::OPENSSL_init_ssl(0, NULL);
        //::OPENSSL_init_crypto(0, NULL);
        (void)SSL_library_init();

        SSL_load_error_strings();

        /* ERR_load_crypto_strings(); */

        // OPENSSL_config(NULL);
#endif
        ::OpenSSL_add_all_algorithms();
    }
    ~Starter()
    {
#ifndef OPENSSL_IS_BORINGSSL
        ::CONF_modules_unload(1);
#endif
    }

    static Starter& the()
    {
        static Starter s;
        return s;
    }
};

//=============================================================================

/*static*/ Context Context::create(const SSL_METHOD* _pm /*= nullptr*/)
{
    Starter::the();
    Context rv;
    if (_pm == nullptr) {
        rv.pctx = ::SSL_CTX_new(::TLS_method());
    } else {
        rv.pctx = ::SSL_CTX_new(_pm);
    }

    SSL_CTX_set_min_proto_version(rv.pctx, 0);
    SSL_CTX_set_max_proto_version(rv.pctx, 0);
    return rv;
}

bool Context::isValid() const
{
    return pctx != nullptr;
}

bool Context::empty() const
{
    return pctx == nullptr;
}

Context::Context(Context&& _rctx) noexcept
{
    pctx       = _rctx.pctx;
    _rctx.pctx = nullptr;
}
Context& Context::operator=(Context&& _rctx) noexcept
{
    if (isValid()) {
        SSL_CTX_free(pctx);
        pctx = nullptr;
    }
    pctx       = _rctx.pctx;
    _rctx.pctx = nullptr;
    return *this;
}
Context::Context()
    : pctx(nullptr)
{
}
Context::~Context()
{
    if (isValid()) {
        SSL_CTX_free(pctx);
        pctx = nullptr;
    }
}

// ErrorCodeT Context::configure(const char *_filename, const char *_appname){
//  ErrorCodeT err;
//  ::ERR_clear_error();
//  if(::CONF_modules_load_file(_filename, _appname, 0) <= 0){
//      err = ssl_category.makeError(::ERR_get_error());
//  }else if(SSL_CTX_config(pctx, "CA_default") == 0){
//      err = ssl_category.makeError(::ERR_get_error());
//  }
//  return err;
// }

namespace {

typedef void (*BIODeleter)(BIO*);
typedef void (*X509Deleter)(X509*);
typedef void (*EVP_PKEYDeleter)(EVP_PKEY*);

void bio_deleter(BIO* _pbio)
{
    if (_pbio != nullptr) {
        ::BIO_free(_pbio);
    }
}

void x509_deleter(X509* _px509)
{
    if (_px509 != nullptr) {
        ::X509_free(_px509);
    }
}

void evp_pkey_deleter(EVP_PKEY* _pkey)
{
    if (_pkey != nullptr) {
        ::EVP_PKEY_free(_pkey);
    }
}

} // namespace

namespace {

inline std::error_code ssl_error()
{
    return ssl_category.makeError(::ERR_get_error());
}

inline std::span<const std::byte> as_bytes(std::string_view _str)
{
    return std::as_bytes(std::span<const char>{_str.data(), _str.size()});
}

//! An error equivalent to the error_stream_shutdown condition - reported when the peer shuts down during a handshake
inline std::error_code peer_shutdown_error()
{
    return std::error_code(error_stream_shutdown.value(), error_stream_shutdown.category());
}

//! The error reported when an operation must be retried once the socket becomes ready
inline std::error_code would_block_error()
{
    return std::make_error_code(std::errc::operation_would_block);
}

} // namespace

std::expected<void, std::error_code> Context::addVerifyAuthority(std::span<const std::byte> _data)
{
    ::ERR_clear_error();

    std::unique_ptr<::BIO, BIODeleter> bio_ptr(::BIO_new_mem_buf(_data.data(), static_cast<int>(_data.size())), bio_deleter);

    if (bio_ptr) {

        std::unique_ptr<::X509, X509Deleter> cert_ptr(::PEM_read_bio_X509(bio_ptr.get(), nullptr, nullptr, nullptr), x509_deleter);

        if (cert_ptr) {
            if (X509_STORE* store = ::SSL_CTX_get_cert_store(pctx)) {
                if (::X509_STORE_add_cert(store, cert_ptr.get()) == 1) {
                    return {};
                }
            }
        }
    }
    return std::unexpected(ssl_error());
}

std::expected<void, std::error_code> Context::addVerifyAuthority(std::string_view _str)
{
    return addVerifyAuthority(as_bytes(_str));
}

std::expected<void, std::error_code> Context::loadDefaultVerifyPaths()
{
    if (SSL_CTX_set_default_verify_paths(pctx) != 0) {
        return {};
    }
    return std::unexpected(wrapper_category.makeError(WrapperError::Call));
}

std::expected<void, std::error_code> Context::loadVerifyFile(std::string_view _path)
{
    const std::string path{_path};
    ::ERR_clear_error();
    if (SSL_CTX_load_verify_locations(pctx, path.c_str(), nullptr) != 1) {
        return std::unexpected(ssl_error());
    }
    return {};
}

std::expected<void, std::error_code> Context::loadVerifyPath(std::string_view _path)
{
    const std::string path{_path};
    ::ERR_clear_error();
    if (SSL_CTX_load_verify_locations(pctx, nullptr, path.c_str()) != 1) {
        return std::unexpected(ssl_error());
    }
    return {};
}

std::expected<void, std::error_code> Context::loadCertificateFile(std::string_view _path, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    const std::string path{_path};
    ::ERR_clear_error();
    if (SSL_CTX_use_certificate_file(pctx, path.c_str(), _fformat == FileFormat::Pem ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1) != 1) {
        return std::unexpected(ssl_error());
    }
    return {};
}

std::expected<void, std::error_code> Context::loadCertificate(std::span<const std::byte> _data, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ::ERR_clear_error();

    if (_fformat == FileFormat::Asn1) {
        if (
            ::SSL_CTX_use_certificate_ASN1(
                pctx,
                static_cast<int>(_data.size()),
                reinterpret_cast<const unsigned char*>(_data.data()))
            == 1) {
            return {};
        }
    } else if (_fformat == FileFormat::Pem) {
        std::unique_ptr<::BIO, BIODeleter> bio_ptr(::BIO_new_mem_buf(_data.data(), static_cast<int>(_data.size())), bio_deleter);
        if (bio_ptr) {
            std::unique_ptr<::X509, X509Deleter> cert_ptr(::PEM_read_bio_X509(bio_ptr.get(), nullptr, nullptr, nullptr), x509_deleter);
            if (cert_ptr) {
                if (::SSL_CTX_use_certificate(pctx, cert_ptr.get()) == 1) {
                    return {};
                }
            }
        }
    } else {
        return std::unexpected(wrapper_category.makeError(WrapperError::Call));
    }
    return std::unexpected(ssl_error());
}

std::expected<void, std::error_code> Context::loadCertificate(std::string_view _str, const FileFormat _fformat)
{
    return loadCertificate(as_bytes(_str), _fformat);
}

std::expected<void, std::error_code> Context::loadPrivateKeyFile(std::string_view _path, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    const std::string path{_path};
    ::ERR_clear_error();
    if (SSL_CTX_use_PrivateKey_file(pctx, path.c_str(), _fformat == FileFormat::Pem ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1) != 1) {
        return std::unexpected(ssl_error());
    }
    solid_assert_log(SSL_CTX_check_private_key(pctx), logger);
    return {};
}

std::expected<void, std::error_code> Context::loadPrivateKey(std::span<const std::byte> _data, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ::ERR_clear_error();

    std::unique_ptr<::BIO, BIODeleter>           bio_ptr(::BIO_new_mem_buf(_data.data(), static_cast<int>(_data.size())), bio_deleter);
    std::unique_ptr<::EVP_PKEY, EVP_PKEYDeleter> key_ptr(nullptr, evp_pkey_deleter);

    if (_fformat == FileFormat::Asn1) {
        key_ptr = std::unique_ptr<::EVP_PKEY, EVP_PKEYDeleter>(::d2i_PrivateKey_bio(bio_ptr.get(), nullptr), evp_pkey_deleter);
    } else if (_fformat == FileFormat::Pem) {
#ifndef OPENSSL_IS_BORINGSSL
        pem_password_cb* callback    = ::SSL_CTX_get_default_passwd_cb(pctx);
        void*            cb_userdata = ::SSL_CTX_get_default_passwd_cb_userdata(pctx);
#else
        pem_password_cb* callback    = nullptr;
        void*            cb_userdata = nullptr;
#endif
        key_ptr = std::unique_ptr<::EVP_PKEY, EVP_PKEYDeleter>(
            ::PEM_read_bio_PrivateKey(
                bio_ptr.get(), nullptr, callback,
                cb_userdata),
            evp_pkey_deleter);
    } else {
        return std::unexpected(wrapper_category.makeError(WrapperError::Call));
    }

    if (key_ptr && ::SSL_CTX_use_PrivateKey(pctx, key_ptr.get()) == 1) {
        return {};
    }
    return std::unexpected(ssl_error());
}

std::expected<void, std::error_code> Context::loadPrivateKey(std::string_view _str, const FileFormat _fformat)
{
    return loadPrivateKey(as_bytes(_str), _fformat);
}

std::expected<void, std::error_code> Context::doSetPasswordCallback()
{
    SSL_CTX_set_default_passwd_cb(pctx, on_password_cb);
    SSL_CTX_set_default_passwd_cb_userdata(pctx, this);

    return {};
}

/*static*/ int Context::on_password_cb(char* buf, int size, int rwflag, void* u)
{
    Context& rthis = *static_cast<Context*>(u);

    buf[0] = 0;

    if (rthis.pwdfnc) {
        std::string pwd = rthis.pwdfnc(size, rwflag == 1 ? PasswordPurpose::Write : PasswordPurpose::Read);
        size_t      sz  = strlen(pwd.c_str());

        if (sz < static_cast<size_t>(size)) {

        } else {
            sz = static_cast<size_t>(size - 1);
        }

        memcpy(buf, pwd.c_str(), sz);
        buf[sz] = 0;
    }

    return static_cast<int>(strlen(buf));
}

//=============================================================================

/*static*/ int Socket::thisSSLDataIndex()
{
    static int idx = SSL_get_ex_new_index(0, (void*)"socket_data", nullptr, nullptr, nullptr);
    return idx;
}
/*static*/ int Socket::contextPointerSSLDataIndex()
{
    static int idx = SSL_get_ex_new_index(0, (void*)"context_data", nullptr, nullptr, nullptr);
    return idx;
}

Socket::Socket(
    const Context& _rctx, SocketDevice&& _rsd)
    : SocketBase(std::move(_rsd))
    , want_read_on_recv(false)
    , want_read_on_send(false)
    , want_write_on_recv(false)
    , want_write_on_send(false)
{
    pssl = SSL_new(_rctx.pctx);
    ::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    if (device()) {
        const int rv = SSL_set_fd(pssl, device().descriptor());
        solid_assert_log(rv != 0, logger);
        (void)rv;
    }
}

Socket::Socket(const Context& _rctx)
    : want_read_on_recv(false)
    , want_read_on_send(false)
    , want_write_on_recv(false)
    , want_write_on_send(false)
{
    pssl = SSL_new(_rctx.pctx);
    ::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
}

Socket::~Socket()
{
    SSL_free(pssl);
}

SocketDevice Socket::reset(ReactorContext& _rctx, SocketDevice&& _rsd)
{

    SocketDevice sd = SocketBase::reset(_rctx, std::move(_rsd));
    if (device()) {
        SSL_set_fd(pssl, sd.descriptor());
    } else {
        SSL_set_fd(pssl, -1);
    }
    return sd;
}

std::expected<void, std::error_code> Socket::create(ReactorContext& _rctx, SocketAddressStub const& _rsas)
{
    const auto rv = SocketBase::create(_rctx, _rsas);

    if (rv) {
        SSL_set_fd(pssl, device().descriptor());
    }
    return rv;
}

std::expected<void, std::error_code> Socket::renegotiate()
{
    ::ERR_clear_error();
    if (::SSL_renegotiate(pssl) == 1) {
        return {};
    }
    return std::unexpected(ssl_error());
}

ReactorEventE Socket::filterReactorEvents(
    const ReactorEventE _evt) const
{
    switch (_evt) {
    case ReactorEventE::Recv:
        // solid_log(logger, Info, "EventRecv "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_read_on_send && want_read_on_recv) {
            return ReactorEventE::SendRecv;
        } else if (want_read_on_send) {
            return ReactorEventE::Send;
        } else if (want_read_on_recv) {
            return ReactorEventE::Recv;
        }
        break;
    case ReactorEventE::Send:
        // solid_log(logger, Info, "EventSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_write_on_send && want_write_on_recv) {
            return ReactorEventE::RecvSend;
        } else if (want_write_on_recv) {
            return ReactorEventE::Recv;
        } else if (want_write_on_send) {
            return ReactorEventE::Send;
        }
        break;
    case ReactorEventE::RecvSend:
        // solid_log(logger, Info, "EventRecvSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_read_on_send && (want_read_on_recv || want_write_on_recv)) {
            return ReactorEventE::SendRecv;
        } else if ((want_write_on_send || want_read_on_send) && (want_write_on_recv || want_read_on_recv)) {
            return _evt;
        } else if (want_write_on_send || want_read_on_send) {
            return ReactorEventE::Send;
        } else if (want_read_on_recv || want_write_on_recv) {
            return ReactorEventE::Recv;
        }
        break;
    default:
        break;
    }
    return _evt;
}

std::expected<int, std::error_code> Socket::doMapSslResult(ReactorContext& _rctx, const int _retval, const std::error_code& _err_sys, const bool _on_recv, const std::error_code& _default_err)
{
    const int           err_cond   = ::SSL_get_error(pssl, _retval);
    const unsigned long err_code   = ::ERR_get_error();
    bool&               want_read  = _on_recv ? want_read_on_recv : want_read_on_send;
    bool&               want_write = _on_recv ? want_write_on_recv : want_write_on_send;

    switch (err_cond) {
    case SSL_ERROR_NONE:
        return _retval;
    case SSL_ERROR_ZERO_RETURN:
        return std::unexpected(peer_shutdown_error());
    case SSL_ERROR_WANT_READ:
        want_read = true;
#if defined(SOLID_USE_WSAPOLL)
        modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Read);
#else
        (void)_rctx;
#endif
        return std::unexpected(would_block_error());
    case SSL_ERROR_WANT_WRITE:
        want_write = true;
#if defined(SOLID_USE_WSAPOLL)
        modifyReactorRequestEvents(_rctx, ReactorWaitRequestE::Write);
#endif
        return std::unexpected(would_block_error());
    case SSL_ERROR_SYSCALL:
        if (_err_sys) {
            return std::unexpected(_err_sys);
        }
        return std::unexpected(solid::error_system);
    case SSL_ERROR_SSL:
        return std::unexpected(ssl_category.makeError(err_code));
    case SSL_ERROR_WANT_X509_LOOKUP:
    default:
        solid_assert_log(false, logger);
        break;
    }
    return std::unexpected(_default_err);
}

std::expected<std::size_t, std::error_code> Socket::recv(ReactorContext& _rctx, std::span<std::byte> _buf)
{
    want_read_on_recv = want_write_on_recv = false;

    storeThisPointer();
    storeContextPointer(&_rctx);

    ::ERR_clear_error();

    const int             retval  = ::SSL_read(pssl, _buf.data(), static_cast<int>(_buf.size()));
    const std::error_code err_sys = last_socket_error();

    clearThisPointer();
    clearContextPointer();

    const auto rv = doMapSslResult(_rctx, retval, err_sys, true, error_secure_recv);

    if (rv) {
        solid_assert_log(*rv >= 0, logger);
        return static_cast<std::size_t>(*rv);
    }
    if (rv.error() == error_stream_shutdown) {
        return 0;
    }
    return std::unexpected(rv.error());
}

std::expected<std::size_t, std::error_code> Socket::send(ReactorContext& _rctx, std::span<const std::byte> _buf)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(&_rctx);

    ::ERR_clear_error();

    const int             retval  = ::SSL_write(pssl, _buf.data(), static_cast<int>(_buf.size()));
    const std::error_code err_sys = last_socket_error();

    clearThisPointer();
    clearContextPointer();

    const auto rv = doMapSslResult(_rctx, retval, err_sys, false, error_secure_send);

    if (rv) {
        solid_assert_log(*rv >= 0, logger);
        return static_cast<std::size_t>(*rv);
    }
    if (rv.error() == error_stream_shutdown) {
        return 0;
    }
    return std::unexpected(rv.error());
}

std::expected<void, std::error_code> Socket::secureAccept(ReactorContext& _rctx)
{
    want_read_on_recv = want_write_on_recv = false;

    storeThisPointer();
    storeContextPointer(&_rctx);

    ::ERR_clear_error();

    const int             retval  = ::SSL_accept(pssl);
    const std::error_code err_sys = last_socket_error();

    clearThisPointer();
    clearContextPointer();

    if (const auto rv = doMapSslResult(_rctx, retval, err_sys, true, error_secure_accept); !rv) {
        return std::unexpected(rv.error());
    }
    return {};
}

std::expected<void, std::error_code> Socket::secureConnect(ReactorContext& _rctx)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(&_rctx);

    ::ERR_clear_error();

    const int             retval  = ::SSL_connect(pssl);
    const std::error_code err_sys = last_socket_error();

    clearThisPointer();
    clearContextPointer();

    solid_log(logger, Verbose, "ssl_connect rv = " << retval);

    if (const auto rv = doMapSslResult(_rctx, retval, err_sys, false, error_secure_connect); !rv) {
        return std::unexpected(rv.error());
    }
    return {};
}

std::expected<void, std::error_code> Socket::secureShutdown(ReactorContext& _rctx)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(&_rctx);

    ::ERR_clear_error();

    const int             retval  = ::SSL_shutdown(pssl);
    const std::error_code err_sys = last_socket_error();

    clearThisPointer();
    clearContextPointer();

    if (const auto rv = doMapSslResult(_rctx, retval, err_sys, false, error_secure_shutdown); !rv) {
        return std::unexpected(rv.error());
    }
    return {};
}

std::expected<std::size_t, std::error_code> Socket::recvFrom(ReactorContext& /*_rctx*/, std::span<std::byte> /*_buf*/, SocketAddress& /*_raddr*/)
{
    return std::unexpected(solid::error_not_implemented);
}

std::expected<std::size_t, std::error_code> Socket::sendTo(ReactorContext& /*_rctx*/, std::span<const std::byte> /*_buf*/, SocketAddressStub const& /*_rsas*/)
{
    return std::unexpected(solid::error_not_implemented);
}

void Socket::storeContextPointer(void* _pctx)
{
    if (pssl != nullptr) {
        SSL_set_ex_data(pssl, contextPointerSSLDataIndex(), _pctx);
    }
}

void Socket::clearContextPointer()
{
    if (pssl != nullptr) {
        SSL_set_ex_data(pssl, contextPointerSSLDataIndex(), nullptr);
    }
}

void Socket::storeThisPointer()
{
    SSL_set_ex_data(pssl, thisSSLDataIndex(), this);
}

void Socket::clearThisPointer()
{
    SSL_set_ex_data(pssl, thisSSLDataIndex(), nullptr);
}

static int convertMask(const VerifyMaskT _verify_mask)
{
    int rv = 0;
    if ((_verify_mask & VerifyModeNone) != 0u) {
        rv |= SSL_VERIFY_NONE;
    }
    if ((_verify_mask & VerifyModePeer) != 0u) {
        rv |= SSL_VERIFY_PEER;
    }
    if ((_verify_mask & VerifyModeFailIfNoPeerCert) != 0u) {
        rv |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
    if ((_verify_mask & VerifyModeClientOnce) != 0u) {
        rv |= SSL_VERIFY_CLIENT_ONCE;
    }
    return rv;
}

std::expected<void, std::error_code> Socket::doPrepareVerifyCallback(VerifyMaskT _verify_mask)
{
    SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);

    return {};
}

/*static*/ int Socket::on_verify(int preverify_ok, X509_STORE_CTX* x509_ctx)
{
    SSL*    ssl   = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    Socket* pthis = static_cast<Socket*>(SSL_get_ex_data(ssl, thisSSLDataIndex()));
    void*   pctx  = SSL_get_ex_data(ssl, contextPointerSSLDataIndex());

    solid_check_log(ssl && pthis, logger);

    if (pthis->verify_cbk) {
        VerifyContext vctx(x509_ctx);

        bool rv = pthis->verify_cbk(pctx, preverify_ok != 0, vctx);
        return rv ? 1 : 0;
    }
    return preverify_ok;
}

std::expected<void, std::error_code> Socket::setVerifyDepth(const int _depth)
{
    SSL_set_verify_depth(pssl, _depth);
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);
    X509_VERIFY_PARAM_set_depth(param, _depth);
    return {};
}

std::expected<void, std::error_code> Socket::setVerifyMode(VerifyMaskT _verify_mask)
{
    SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);
    return {};
}

std::expected<void, std::error_code> Socket::setCheckHostName(std::string_view _hostname)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);

    // X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (X509_VERIFY_PARAM_set1_host(param, _hostname.data(), _hostname.size()) != 0) { // boringssl requires namelen to be nonzero
        return {};
    }
    return std::unexpected(wrapper_category.makeError(WrapperError::SetCheckHostName));
}

std::expected<void, std::error_code> Socket::setCheckEmail(std::string_view _email)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);

    if (X509_VERIFY_PARAM_set1_email(param, _email.data(), _email.size()) != 0) { // boringssl requires emaillen to be nonzero
        return {};
    }
    return std::unexpected(wrapper_category.makeError(WrapperError::SetCheckEmail));
}

std::expected<void, std::error_code> Socket::setCheckIP(std::string_view _ip)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);
    const std::string  ip{_ip};

    if (X509_VERIFY_PARAM_set1_ip_asc(param, ip.c_str()) != 0) {
        return {};
    }

    return std::unexpected(wrapper_category.makeError(WrapperError::SetCheckIP));
}

} // namespace solid::frame::aio::openssl
