cmake_minimum_required(version 3.11)
project(jw-dpu)

#
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#
find_package(Boost 1.66 REQUIRED COMPONENTS program_options)

#
add_subdirectory(libaio_test)
