link_directories(/usr/lib)

add_executable(mb_unit_server unit-test-server.c)
target_link_libraries(mb_unit_server PRIVATE libmodbus.so.5.1.0)


add_executable(mb_info version.c)
target_link_libraries(mb_info PRIVATE libmodbus.so.5.1.0)
