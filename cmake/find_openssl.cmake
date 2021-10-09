set(OPENSSL_ROOT_DIR ${EXTERNAL_DIR})

find_package(OpenSSL)

if(SOLID_ON_WINDOWS)
    file( GLOB OPENSSL_SSL_DLL "${EXTERNAL_DIR}/bin/libssl-*.dll")
    file( GLOB OPENSSL_CRYPTO_DLL "${EXTERNAL_DIR}/bin/libcrypto-*.dll")

    message("OpenSSL DLLs ${OPENSSL_SSL_DLL} ${OPENSSL_CRYPTO_DLL}")
endif()

if(OPENSSL_FOUND)
    message("\n-- OpenSSL version: ${OPENSSL_VERSION}\n")
else()
    message("\n-- OpenSSL not found\n")
endif()
