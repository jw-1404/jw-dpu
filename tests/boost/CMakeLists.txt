# program options
add_executable(po_first po_first.cpp)
target_link_libraries(po_first PRIVATE Boost::program_options)

# asio
add_executable(chat_client chat_client.cpp)
target_link_libraries(chat_client PRIVATE Threads::Threads Boost::system)

add_executable(chat_server chat_server.cpp)
target_link_libraries(chat_server PRIVATE Threads::Threads Boost::system)

