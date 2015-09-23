FIND_LIBRARY(OPENSSL_CRYPTO_LIB
	NAMES
		libeay32
		crypto
	PATHS
		${EXTERN_PATH}/lib NO_DEFAULT_PATH
)
FIND_LIBRARY(OPENSSL_SSL_LIB
		NAMES
			ssleay32
			ssl
		PATHS
			${EXTERN_PATH}/lib NO_DEFAULT_PATH
)

SET(OPENSSL_LIBS ${OPENSSL_SSL_LIB} ${OPENSSL_CRYPTO_LIB})

MACRO (OSSL_COPY_DLLS trgt)
    if(WIN32)
        FILE(GLOB DepDlls ${OSSL_PATH}/lib/*.dll)
        #MESSAGE ("dependent dlls: " DepDlls [${DepDlls}])
        #MESSAGE ("current folder: "${CMAKE_CURRENT_BINARY_DIR}/)
        #MESSAGE ("cmake command: "${CMAKE_COMMAND})
        FOREACH (depDll ${DepDlls})
            #MESSAGE ("cmake.exe -E copy ${depDll} ${CMAKE_CURRENT_BINARY_DIR}")
            ADD_CUSTOM_COMMAND (
                TARGET ${trgt}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                ARGS -E copy ${depDll} ${CMAKE_CURRENT_BINARY_DIR}
        )
        ENDFOREACH (depDll)
    endif(WIN32)
ENDMACRO (OSSL_COPY_DLLS)
