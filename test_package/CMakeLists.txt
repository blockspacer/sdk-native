cmake_minimum_required(VERSION 3.5)
project(PackageTest)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(example example.c)
target_link_libraries(example CONAN_PKG::tanker)
