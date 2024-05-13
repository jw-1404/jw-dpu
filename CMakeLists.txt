cmake_minimum_required(VERSION 3.11)
project(jw-dpu)

#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#
find_package(Boost 1.66 REQUIRED COMPONENTS program_options system)

#
add_subdirectory(libaio_test)
add_subdirectory(lib)
add_subdirectory(src)
