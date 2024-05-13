add_executable(demo_libaio main.cpp)
target_link_libraries(demo_libaio PRIVATE aio Boost::program_options)

add_executable(demo_first first.cpp)
target_link_libraries(demo_first PRIVATE aio Boost::program_options)
