include_directories(${SOLIDGROUND_SOURCE_DIR}/test/clientserver)
add_library(test_alpha src/alphaconnection.cpp src/alphaservice.cpp src/alphacommands.cpp src/alphareader.cpp src/alphawriter.cpp)
