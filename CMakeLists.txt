cmake_minimum_required(VERSION 3.11)
project(jw-dpu)

#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#
find_package(Threads)
find_package(Boost REQUIRED COMPONENTS program_options system)
if(Boost_FOUND)
  message("Boost_INCLUDE_DIRS=${Boost_INCLUDE_DIRS}")
  include_directories(${Boost_INCLUDE_DIRS})
endif()

#
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)
