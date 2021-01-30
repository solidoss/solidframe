set(OPENSSL_ROOT_DIR ${EXTERNAL_DIR})

find_package(OpenSSL)

if(OPENSSL_FOUND)
    message("\n-- OpenSSL version: ${OPENSSL_VERSION}\n")
else()
    message("\n-- OpenSSL not found\n")
endif()
