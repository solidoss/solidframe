if(ON_ANDROID)
    set(OPENSSL_ROOT_DIR "${EXTERN_PATH}/${ANDROID_ABI}")
else()
    set(OPENSSL_ROOT_DIR "${EXTERN_PATH}")
endif()

find_package(OpenSSL)
