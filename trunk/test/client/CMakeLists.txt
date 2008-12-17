#include_directories (${SOLIDGROUND_SOURCE_DIR})

#link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (alphaclienta alphaclienta.cpp writer.cpp)
add_executable (alphaclients alphaclients.cpp writer.cpp)
add_executable (alphafetch alphafetch.cpp writer.cpp)
add_executable (alphastore alphastore.cpp writer.cpp)

target_link_libraries (alphaclienta utility system ssl crypto pthread rt dl)
target_link_libraries (alphaclients utility system ssl crypto pthread rt dl)
target_link_libraries (alphafetch utility system ssl crypto  pthread rt dl)
target_link_libraries (alphastore utility system ssl crypto  pthread rt dl)
