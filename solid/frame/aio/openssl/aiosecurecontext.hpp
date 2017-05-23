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

#include "openssl/ssl.h"
#include "solid/system/error.hpp"
#include "solid/system/function.hpp"

namespace solid {
namespace frame {
namespace aio {
namespace openssl {

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
public:
    using NativeContextT = SSL_CTX*;
    static Context create(const SSL_METHOD* = nullptr);

    Context();

    Context(Context const&) = delete;

    Context(Context&& _rctx);

    Context& operator=(Context const&) = delete;

    Context& operator=(Context&& _rctx);

    ~Context();

    bool isValid() const;

    bool empty() const;

    //ErrorCodeT configure(const char *_filename = nullptr, const char *_appname = nullptr);

    ErrorCodeT addVerifyAuthority(const unsigned char* _data, const size_t _data_size);
    ErrorCodeT addVerifyAuthority(const std::string& _str);

    ErrorCodeT loadDefaultVerifyPaths();

    //!Use it on client side to load the certificates
    ErrorCodeT loadVerifyFile(const char* _path);
    //!Use it on client side to load the certificates
    ErrorCodeT loadVerifyPath(const char* _path);

    //!Use it on client/server side to load the certificates
    ErrorCodeT loadCertificateFile(const char* _path, const FileFormat _fformat = FileFormat::Pem);
    ErrorCodeT loadCertificate(const unsigned char* _data, const size_t _data_size, const FileFormat _fformat = FileFormat::Pem);
    ErrorCodeT loadCertificate(const std::string& _str, const FileFormat _fformat = FileFormat::Pem);
    //!Use it on client/server side to load the certificates
    ErrorCodeT loadPrivateKeyFile(const char* _path, const FileFormat _fformat = FileFormat::Pem);
    ErrorCodeT loadPrivateKey(const unsigned char* _data, const size_t _data_size, const FileFormat _fformat = FileFormat::Pem);
    ErrorCodeT loadPrivateKey(const std::string& _str, const FileFormat _fformat = FileFormat::Pem);

    template <typename F>
    ErrorCodeT passwordCallback(F _f)
    {
        pwdfnc = _f;
        return doSetPasswordCallback();
    }

    NativeContextT nativeContext() const
    {
        return pctx;
    }

private:
    static int on_password_cb(char* buf, int size, int rwflag, void* u);
    ErrorCodeT doSetPasswordCallback();

private:
    using PasswordFunctionT = SOLID_FUNCTION<std::string(std::size_t, PasswordPurpose)>;

    friend class Socket;
    SSL_CTX*          pctx;
    PasswordFunctionT pwdfnc;
};

} //namespace openssl
} //namespace aio
} //namespace frame
} //namespace solid
