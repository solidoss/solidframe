if(ON_ANDROID)
    set(OPENSSL_ROOT_DIR "${EXTERNAL_DIR}/${ANDROID_ABI}")
else()
    set(OPENSSL_ROOT_DIR ${EXTERNAL_DIR})
endif()

find_package(OpenSSL)
