if(ON_ANDROID)
    set(OPENSSL_ROOT_DIR "${EXTERNAL_PATH}/${ANDROID_ABI}")
else()
    set(OPENSSL_ROOT_DIR "${EXTERNAL_PATH}")
endif()

find_package(OpenSSL)
