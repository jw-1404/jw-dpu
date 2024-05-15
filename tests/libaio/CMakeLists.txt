add_executable(demo_libaio demo_libaio.cpp)
target_link_libraries(demo_libaio PRIVATE aio Boost::program_options)

#####
add_executable(aiocp aiocp.c)
target_link_libraries(aiocp PRIVATE aio)

