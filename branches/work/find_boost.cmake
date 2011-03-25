FIND_LIBRARY(BOOST_TIME
                NAMES
					libboost_date_time.lib
                    libboost_date_time.a
                    boost_date_time
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)

FIND_LIBRARY(BOOST_PROGRAM_OPTIONS
                NAMES
                    libboost_program_options.lib
                    libboost_program_options.a
                    boost_program_options
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)

FIND_LIBRARY(BOOST_THREAD
                NAMES
					libboost_thread.lib
                    libboost_thread.a
                    boost_thread
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)

FIND_LIBRARY(BOOST_TEST
                NAMES
                    libboost_unit_test_framework.lib
                    libboost_unit_test_framework.a
                    boost_unit_test_framework
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)

FIND_LIBRARY(BOOST_FILESYSTEM
                NAMES
                    libboost_filesystem.lib
                    libboost_filesystem.a
                    boost_filesystem
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)

FIND_LIBRARY(BOOST_SYSTEM
                NAMES
					libboost_system.lib
                    libboost_system.a
                    boost_system
                PATHS
                    ${EXTERN_PATH}/lib
                NO_DEFAULT_PATH)
