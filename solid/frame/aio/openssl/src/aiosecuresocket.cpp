// solid/frame/aio/openssl/src/aiocompletion.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"

#include "solid/frame/aio/aioerror.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include <mutex>
#include <thread>

//#include <cstdio>

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
    const char* name() const noexcept
    {
        return "OpenSSL";
    }
    std::string message(int _ev) const;

    solid::ErrorCodeT makeError(unsigned long _err) const
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
    const char* name() const noexcept
    {
        return "solid::frame::aio::openssl";
    }
    std::string message(int _ev) const;

    solid::ErrorCodeT makeError(WrapperError _err) const
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

} //namespace

namespace solid {
namespace frame {
namespace aio {
namespace openssl {

struct Starter {

    Starter()
    {
#ifndef OPENSSL_IS_BORINGSSL
        ::OPENSSL_init_ssl(0, NULL);
        ::OPENSSL_init_crypto(0, NULL);
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

Context::Context(Context&& _rctx)
{
    pctx       = _rctx.pctx;
    _rctx.pctx = nullptr;
}
Context& Context::operator=(Context&& _rctx)
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
    if (_pbio)
        ::BIO_free(_pbio);
}

void x509_deleter(X509* _px509)
{
    if (_px509)
        ::X509_free(_px509);
}

void evp_pkey_deleter(EVP_PKEY* _pkey)
{
    if (_pkey)
        ::EVP_PKEY_free(_pkey);
}

} //namespace

ErrorCodeT Context::addVerifyAuthority(const unsigned char* _data, const size_t _data_size)
{
    ErrorCodeT err;
    ::ERR_clear_error();

    std::unique_ptr<::BIO, BIODeleter> bio_ptr(::BIO_new_mem_buf(_data, static_cast<int>(_data_size)), bio_deleter);

    if (bio_ptr) {

        std::unique_ptr<::X509, X509Deleter> cert_ptr(::PEM_read_bio_X509(bio_ptr.get(), 0, 0, 0), x509_deleter);

        if (cert_ptr) {
            if (X509_STORE* store = ::SSL_CTX_get_cert_store(pctx)) {
                if (::X509_STORE_add_cert(store, cert_ptr.get()) == 1) {
                    return err;
                }
            }
        }
    }
    return ssl_category.makeError(::ERR_get_error());
}

ErrorCodeT Context::addVerifyAuthority(const std::string& _str)
{
    return addVerifyAuthority(reinterpret_cast<const unsigned char*>(_str.data()), _str.size());
}

ErrorCodeT Context::loadDefaultVerifyPaths()
{
    if (SSL_CTX_set_default_verify_paths(pctx)) {
        return ErrorCodeT();
    } else {
        return wrapper_category.makeError(WrapperError::Call);
    }
}

ErrorCodeT Context::loadVerifyFile(const char* _path)
{
    ErrorCodeT err;
    ::ERR_clear_error();
    if (SSL_CTX_load_verify_locations(pctx, _path, nullptr)) {
        err = ssl_category.makeError(::ERR_get_error());
    }
    return err;
}
ErrorCodeT Context::loadVerifyPath(const char* _path)
{
    ErrorCodeT err;
    ::ERR_clear_error();
    if (SSL_CTX_load_verify_locations(pctx, nullptr, _path)) {
        err = ssl_category.makeError(::ERR_get_error());
    }
    return err;
}

ErrorCodeT Context::loadCertificateFile(const char* _path, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ErrorCodeT err;
    ::ERR_clear_error();
    if (SSL_CTX_use_certificate_file(pctx, _path, _fformat == FileFormat::Pem ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1)) {
        err = ssl_category.makeError(::ERR_get_error());
    }
    return err;
}

ErrorCodeT Context::loadCertificate(const unsigned char* _data, const size_t _data_size, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ErrorCodeT err;
    ::ERR_clear_error();

    if (_fformat == FileFormat::Asn1) {
        if (
            ::SSL_CTX_use_certificate_ASN1(
                pctx,
                static_cast<int>(_data_size),
                _data)
            == 1) {
            return err;
        }
    } else if (_fformat == FileFormat::Pem) {
        std::unique_ptr<::BIO, BIODeleter> bio_ptr(::BIO_new_mem_buf(_data, static_cast<int>(_data_size)), bio_deleter);
        if (bio_ptr) {
            std::unique_ptr<::X509, X509Deleter> cert_ptr(::PEM_read_bio_X509(bio_ptr.get(), 0, 0, 0), x509_deleter);
            if (cert_ptr) {
                if (::SSL_CTX_use_certificate(pctx, cert_ptr.get()) == 1) {
                    return err;
                }
            }
        }
    } else {
        return wrapper_category.makeError(WrapperError::Call);
    }
    return ssl_category.makeError(::ERR_get_error());
}

ErrorCodeT Context::loadCertificate(const std::string& _str, const FileFormat _fformat)
{
    return loadCertificate(reinterpret_cast<const unsigned char*>(_str.data()), _str.size(), _fformat);
}

ErrorCodeT Context::loadPrivateKeyFile(const char* _path, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ErrorCodeT err;
    ::ERR_clear_error();
    if (SSL_CTX_use_PrivateKey_file(pctx, _path, _fformat == FileFormat::Pem ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1)) {
        err = ssl_category.makeError(::ERR_get_error());
    }
    return err;
}

ErrorCodeT Context::loadPrivateKey(const unsigned char* _data, const size_t _data_size, const FileFormat _fformat /* = FileFormat::Pem*/)
{
    ErrorCodeT err;
    ::ERR_clear_error();

    std::unique_ptr<::BIO, BIODeleter>           bio_ptr(::BIO_new_mem_buf(_data, static_cast<int>(_data_size)), bio_deleter);
    std::unique_ptr<::EVP_PKEY, EVP_PKEYDeleter> key_ptr(nullptr, evp_pkey_deleter);

    if (_fformat == FileFormat::Asn1) {
        key_ptr = std::unique_ptr<::EVP_PKEY, EVP_PKEYDeleter>(::d2i_PrivateKey_bio(bio_ptr.get(), 0), evp_pkey_deleter);
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
                bio_ptr.get(), 0, callback,
                cb_userdata),
            evp_pkey_deleter);
    } else {
        return wrapper_category.makeError(WrapperError::Call);
    }
    return ssl_category.makeError(::ERR_get_error());
}

ErrorCodeT Context::loadPrivateKey(const std::string& _str, const FileFormat _fformat)
{
    return loadPrivateKey(reinterpret_cast<const unsigned char*>(_str.data()), _str.size(), _fformat);
}

ErrorCodeT Context::doSetPasswordCallback()
{
    SSL_CTX_set_default_passwd_cb(pctx, on_password_cb);
    SSL_CTX_set_default_passwd_cb_userdata(pctx, this);

    return ErrorCodeT();
}

/*static*/ int Context::on_password_cb(char* buf, int size, int rwflag, void* u)
{
    Context& rthis = *static_cast<Context*>(u);

    buf[0] = 0;

    if (!SOLID_FUNCTION_EMPTY(rthis.pwdfnc)) {
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
    : sd(std::move(_rsd))
    , want_read_on_recv(false)
    , want_read_on_send(false)
    , want_write_on_recv(false)
    , want_write_on_send(false)
{
    pssl = SSL_new(_rctx.pctx);
    ::SSL_set_mode(pssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    ::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    if (sd) {
        sd.makeNonBlocking();
        SSL_set_fd(pssl, sd.descriptor());
    }
}

Socket::Socket(const Context& _rctx)
    : want_read_on_recv(false)
    , want_read_on_send(false)
    , want_write_on_recv(false)
    , want_write_on_send(false)
{
    pssl = SSL_new(_rctx.pctx);
    ::SSL_set_mode(pssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    ::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
}

Socket::~Socket()
{
    SSL_free(pssl);
}

SocketDevice Socket::reset(const Context& _rctx, SocketDevice&& _rsd, ErrorCodeT& _rerr)
{
    SocketDevice tmpsd = std::move(sd);
    sd                 = std::move(_rsd);
    if (sd) {
        sd.makeNonBlocking();
        SSL_set_fd(pssl, sd.descriptor());
    } else {
        SSL_set_fd(pssl, -1);
    }
    return tmpsd;
}

void Socket::shutdown()
{
    sd.shutdownReadWrite();
}

SocketDevice const& Socket::device() const
{
    return sd;
}

SocketDevice& Socket::device()
{
    return sd;
}

bool Socket::create(SocketAddressStub const& _rsas, ErrorCodeT& _rerr)
{
    _rerr = sd.create(_rsas.family());
    if (!_rerr) {
        _rerr = sd.makeNonBlocking();
    }
    if (!_rerr) {
        SSL_set_fd(pssl, sd.descriptor());
    }
    return !_rerr;
}

bool Socket::connect(SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
{
    _rerr = sd.connect(_rsas, _can_retry);
    return !_rerr;
}

ErrorCodeT Socket::renegotiate(bool& _can_retry)
{
    const int           retval   = ::SSL_renegotiate(pssl);
    const unsigned long err_code = ::ERR_get_error();
    (void)retval;
    return ssl_category.makeError(err_code);
}

ReactorEventsE Socket::filterReactorEvents(
    const ReactorEventsE _evt) const
{
    switch (_evt) {
    case ReactorEventRecv:
        //idbgx(Debug::aio, "EventRecv "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_read_on_send && want_read_on_recv) {
            return ReactorEventSendRecv;
        } else if (want_read_on_send) {
            return ReactorEventSend;
        } else if (want_read_on_recv) {
            return ReactorEventRecv;
        }
        break;
    case ReactorEventSend:
        //idbgx(Debug::aio, "EventSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_write_on_send && want_write_on_recv) {
            return ReactorEventRecvSend;
        } else if (want_write_on_recv) {
            return ReactorEventRecv;
        } else if (want_write_on_send) {
            return ReactorEventSend;
        }
        break;
    case ReactorEventRecvSend:
        //idbgx(Debug::aio, "EventRecvSend "<<want_read_on_send<<' '<<want_read_on_recv<<' '<<want_write_on_send<<' '<<want_write_on_recv);
        if (want_read_on_send && (want_read_on_recv || want_write_on_recv)) {
            return ReactorEventSendRecv;
        } else if ((want_write_on_send || want_write_on_send) && (want_write_on_recv || want_read_on_recv)) {
            return _evt;
        } else if (want_write_on_send || want_read_on_send) {
            return ReactorEventSend;
        } else if (want_read_on_recv || want_write_on_recv) {
            return ReactorEventRecv;
        }
        break;
    default:
        break;
    }
    return _evt;
}

ssize_t Socket::recv(void* _pctx, char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
{
    want_read_on_recv = want_write_on_recv = false;

    storeThisPointer();
    storeContextPointer(_pctx);

    ::ERR_clear_error();

    const int           retval   = ::SSL_read(pssl, _pb, static_cast<int>(_bl));
    const ErrorCodeT    err_sys  = last_socket_error();
    const int           err_cond = ::SSL_get_error(pssl, retval);
    const unsigned long err_code = ::ERR_get_error();

    clearThisPointer();
    clearContextPointer();

    switch (err_cond) {
    case SSL_ERROR_NONE:
        _can_retry = false;
        SOLID_ASSERT(retval >= 0);
        return retval;
    case SSL_ERROR_ZERO_RETURN:
        _can_retry = false;
        return 0;
    case SSL_ERROR_WANT_READ:
        _can_retry        = true;
        want_read_on_recv = true;
        return -1;
    case SSL_ERROR_WANT_WRITE:
        _can_retry         = true;
        want_write_on_recv = true;
        return -1;
    case SSL_ERROR_SYSCALL:
        _can_retry = false;
        if (err_sys) {
            _rerr = err_sys;
        } else {
            //TODO: find out why this happens
            _rerr = solid::error_system;
        }
        SOLID_ASSERT(_rerr);
        break;
    case SSL_ERROR_SSL:
        _can_retry = false;
        _rerr      = ssl_category.makeError(err_code);
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
    //for reschedule, we can return -1 but not set the _rerr
    default:
        SOLID_ASSERT(false);
        break;
    }
    return -1;
}

ssize_t Socket::send(void* _pctx, const char* _pb, size_t _bl, bool& _can_retry, ErrorCodeT& _rerr)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(_pctx);

    ::ERR_clear_error();

    const int           retval   = ::SSL_write(pssl, _pb, static_cast<int>(_bl));
    const ErrorCodeT    err_sys  = last_socket_error();
    const int           err_cond = ::SSL_get_error(pssl, retval);
    const unsigned long err_code = ::ERR_get_error();

    clearThisPointer();
    clearContextPointer();

    switch (err_cond) {
    case SSL_ERROR_NONE:
        _can_retry = false;
        return retval;
    case SSL_ERROR_ZERO_RETURN:
        _can_retry = false;
        return 0;
    case SSL_ERROR_WANT_READ:
        _can_retry        = true;
        want_read_on_send = true;
        return -1;
    case SSL_ERROR_WANT_WRITE:
        _can_retry         = true;
        want_write_on_send = true;
        return -1;
    case SSL_ERROR_SYSCALL:
        _can_retry = false;
        if (err_sys) {
            _rerr = err_sys;
        } else {
            //TODO: find out why this happens
            _rerr = solid::error_system;
        }
        break;
    case SSL_ERROR_SSL:
        _can_retry = false;
        _rerr      = ssl_category.makeError(err_code);
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
    //for reschedule, we can return -1 but not set the _rerr
    default:
        SOLID_ASSERT(false);
        break;
    }
    return -1;
}

bool Socket::secureAccept(void* _pctx, bool& _can_retry, ErrorCodeT& _rerr)
{
    want_read_on_recv = want_write_on_recv = false;

    storeThisPointer();
    storeContextPointer(_pctx);

    ::ERR_clear_error();

    const int           retval   = ::SSL_accept(pssl);
    const ErrorCodeT    err_sys  = last_socket_error();
    const int           err_cond = ::SSL_get_error(pssl, retval);
    const unsigned long err_code = ::ERR_get_error();

    clearThisPointer();
    clearContextPointer();

    switch (err_cond) {
    case SSL_ERROR_NONE:
        _can_retry = false;
        return true;
    case SSL_ERROR_WANT_READ:
        _can_retry        = true;
        want_read_on_recv = true;
        return false;
    case SSL_ERROR_WANT_WRITE:
        _can_retry         = true;
        want_write_on_recv = true;
        return false;
    case SSL_ERROR_SYSCALL:
        _can_retry = false;
        if (err_sys) {
            _rerr = err_sys;
        } else {
            //TODO: find out why this happens
            _rerr = solid::error_system;
        }
        break;
    case SSL_ERROR_SSL:
        _can_retry = false;
        _rerr      = ssl_category.makeError(err_code);
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
    //for reschedule, we can return -1 but not set the _rerr
    default:
        SOLID_ASSERT(false);
        break;
    }
    return false;
}

bool Socket::secureConnect(void* _pctx, bool& _can_retry, ErrorCodeT& _rerr)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(_pctx);

    ::ERR_clear_error();

    const int           retval   = ::SSL_connect(pssl);
    const ErrorCodeT    err_sys  = last_socket_error();
    const int           err_cond = ::SSL_get_error(pssl, retval);
    const unsigned long err_code = ::ERR_get_error();

    clearThisPointer();
    clearContextPointer();

    vdbgx(Debug::aio, "ssl_connect rv = " << retval << " ssl_error " << err_cond);

    switch (err_cond) {
    case SSL_ERROR_NONE:
        _can_retry = false;
        return true;
    case SSL_ERROR_WANT_READ:
        _can_retry        = true;
        want_read_on_send = true;
        return false;
    case SSL_ERROR_WANT_WRITE:
        _can_retry         = true;
        want_write_on_send = true;
        return false;
    case SSL_ERROR_SYSCALL:
        _can_retry = false;
        if (err_sys) {
            _rerr = err_sys;
        } else {
            //TODO: find out why this happens
            _rerr = solid::error_system;
        }
        break;
    case SSL_ERROR_SSL:
        _can_retry = false;
        _rerr      = ssl_category.makeError(err_code);
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
    //for reschedule, we can return -1 but not set the _rerr
    default:
        SOLID_ASSERT(false);
        break;
    }
    return false;
}

bool Socket::secureShutdown(void* _pctx, bool& _can_retry, ErrorCodeT& _rerr)
{
    want_read_on_send = want_write_on_send = false;

    storeThisPointer();
    storeContextPointer(_pctx);

    ::ERR_clear_error();

    const int           retval   = ::SSL_shutdown(pssl);
    const ErrorCodeT    err_sys  = last_socket_error();
    const int           err_cond = ::SSL_get_error(pssl, retval);
    const unsigned long err_code = ::ERR_get_error();

    clearThisPointer();
    clearContextPointer();

    switch (err_cond) {
    case SSL_ERROR_NONE:
        _can_retry = false;
        return true;
    case SSL_ERROR_WANT_READ:
        _can_retry        = true;
        want_read_on_send = true;
        return false;
    case SSL_ERROR_WANT_WRITE:
        _can_retry         = true;
        want_write_on_send = true;
        return false;
    case SSL_ERROR_SYSCALL:
        _can_retry = false;
        if (err_sys) {
            _rerr = err_sys;
        } else {
            //TODO: find out why this happens
            _rerr = solid::error_system;
        }
        break;
    case SSL_ERROR_SSL:
        _can_retry = false;
        _rerr      = ssl_category.makeError(err_code);
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
    //for reschedule, we can return -1 but not set the _rerr
    default:
        SOLID_ASSERT(false);
        break;
    }
    return false;
}

ssize_t Socket::recvFrom(char* _pb, size_t _bl, SocketAddress& _addr, bool& _can_retry, ErrorCodeT& _rerr)
{
    return -1;
}

ssize_t Socket::sendTo(const char* _pb, size_t _bl, SocketAddressStub const& _rsas, bool& _can_retry, ErrorCodeT& _rerr)
{
    return -1;
}

void Socket::storeContextPointer(void* _pctx)
{
    if (pssl) {
        SSL_set_ex_data(pssl, contextPointerSSLDataIndex(), _pctx);
    }
}

void Socket::clearContextPointer()
{
    if (pssl) {
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
    if (_verify_mask & VerifyModeNone)
        rv |= SSL_VERIFY_NONE;
    if (_verify_mask & VerifyModePeer)
        rv |= SSL_VERIFY_PEER;
    if (_verify_mask & VerifyModeFailIfNoPeerCert)
        rv |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    if (_verify_mask & VerifyModeClientOnce)
        rv |= SSL_VERIFY_CLIENT_ONCE;
    return rv;
}

ErrorCodeT Socket::doPrepareVerifyCallback(VerifyMaskT _verify_mask)
{
    SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);

    return ErrorCodeT();
}

/*static*/ int Socket::on_verify(int preverify_ok, X509_STORE_CTX* x509_ctx)
{
    SSL*    ssl   = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    Socket* pthis = static_cast<Socket*>(SSL_get_ex_data(ssl, thisSSLDataIndex()));
    void*   pctx  = SSL_get_ex_data(ssl, contextPointerSSLDataIndex());

    SOLID_CHECK(ssl);
    SOLID_CHECK(pthis);

    if (!SOLID_FUNCTION_EMPTY(pthis->verify_cbk)) {
        VerifyContext vctx(x509_ctx);

        bool rv = pthis->verify_cbk(pctx, preverify_ok != 0, vctx);
        return rv ? 1 : 0;
    } else {
        return preverify_ok;
    }
}

ErrorCodeT Socket::setVerifyDepth(const int _depth)
{
    SSL_set_verify_depth(pssl, _depth);
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);
    ;
    X509_VERIFY_PARAM_set_depth(param, _depth);
    return ErrorCodeT();
}

ErrorCodeT Socket::setVerifyMode(VerifyMaskT _verify_mask)
{
    SSL_set_verify(pssl, convertMask(_verify_mask), on_verify);
    return ErrorCodeT();
}

ErrorCodeT Socket::setCheckHostName(const std::string& _hostname)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);

    //X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (X509_VERIFY_PARAM_set1_host(param, _hostname.c_str(), 0)) {
        return ErrorCodeT();
    } else {
        return wrapper_category.makeError(WrapperError::SetCheckHostName);
    }
}

ErrorCodeT Socket::setCheckEmail(const std::string& _email)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);
    ;
    if (X509_VERIFY_PARAM_set1_email(param, _email.c_str(), 0)) {
        return ErrorCodeT();
    } else {
        return wrapper_category.makeError(WrapperError::SetCheckEmail);
    }
}

ErrorCodeT Socket::setCheckIP(const std::string& _ip)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(pssl);
    ;
    if (X509_VERIFY_PARAM_set1_ip_asc(param, _ip.c_str())) {
        return ErrorCodeT();
    } else {
        return wrapper_category.makeError(WrapperError::SetCheckIP);
    }
}

} //namespace openssl
} //namespace aio
} //namespace frame
} //namespace solid
