// solid/frame/aio/openssl/aiosecurecontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/utility/common.hpp"
#ifdef SOLID_ON_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#include "openssl/ssl.h"
#include "solid/system/error.hpp"
#include "solid/utility/function.hpp"
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace solid::frame::aio::openssl {

enum struct PasswordPurpose {
    Read,
    Write
};

enum struct FileFormat {
    Asn1,
    Pem
};

class Socket;
class Context {
    Context();

public:
    using NativeContextT = SSL_CTX*;
    static Context create(const SSL_METHOD* = nullptr);

    Context(Context const&) = delete;

    Context(Context&& _rctx) noexcept;

    Context& operator=(Context const&) = delete;

    Context& operator=(Context&& _rctx) noexcept;

    ~Context();

    bool isValid() const;

    bool empty() const;

    //! Add a PEM encoded certificate authority to the verify store
    [[nodiscard]] std::expected<void, std::error_code> addVerifyAuthority(std::span<const std::byte> _data);
    [[nodiscard]] std::expected<void, std::error_code> addVerifyAuthority(std::string_view _str);

    [[nodiscard]] std::expected<void, std::error_code> loadDefaultVerifyPaths();

    //! Use it on client side to load the certificates
    [[nodiscard]] std::expected<void, std::error_code> loadVerifyFile(std::string_view _path);
    //! Use it on client side to load the certificates
    [[nodiscard]] std::expected<void, std::error_code> loadVerifyPath(std::string_view _path);

    //! Use it on client/server side to load the certificates
    [[nodiscard]] std::expected<void, std::error_code> loadCertificateFile(std::string_view _path, const FileFormat _fformat = FileFormat::Pem);
    [[nodiscard]] std::expected<void, std::error_code> loadCertificate(std::span<const std::byte> _data, const FileFormat _fformat = FileFormat::Pem);
    [[nodiscard]] std::expected<void, std::error_code> loadCertificate(std::string_view _str, const FileFormat _fformat = FileFormat::Pem);
    //! Use it on client/server side to load the private key
    [[nodiscard]] std::expected<void, std::error_code> loadPrivateKeyFile(std::string_view _path, const FileFormat _fformat = FileFormat::Pem);
    [[nodiscard]] std::expected<void, std::error_code> loadPrivateKey(std::span<const std::byte> _data, const FileFormat _fformat = FileFormat::Pem);
    [[nodiscard]] std::expected<void, std::error_code> loadPrivateKey(std::string_view _str, const FileFormat _fformat = FileFormat::Pem);

    template <typename F>
    [[nodiscard]] std::expected<void, std::error_code> passwordCallback(F _f)
    {
        pwdfnc = _f;
        return doSetPasswordCallback();
    }

    NativeContextT nativeContext() const
    {
        return pctx;
    }

private:
    static int                           on_password_cb(char* buf, int size, int rwflag, void* u);
    std::expected<void, std::error_code> doSetPasswordCallback();

private:
    using PasswordFunctionT = Function<std::string(std::size_t, PasswordPurpose)>;

    friend class Socket;
    SSL_CTX*          pctx;
    PasswordFunctionT pwdfnc;
};

} // namespace solid::frame::aio::openssl
