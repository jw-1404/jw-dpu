add_executable(jw_to_device dma_to_device.c)
target_link_libraries(jw_to_device PUBLIC dma_util)
target_include_directories(jw_to_device PUBLIC
${PROJECT_SOURCE_DIR}/lib)

add_executable(dma_from_device dma_from_device.c)
target_link_libraries(dma_from_device PUBLIC dma_util)
target_include_directories(dma_from_device PUBLIC ${PROJECT_SOURCE_DIR}/lib)

add_executable(jw_from_device jw_from_device.cpp)
target_link_libraries(jw_from_device PUBLIC Boost::program_options)

add_executable(reg_rw reg_rw.c)
add_executable(test_chrdev test_chrdev.c)

############################ 
add_executable(asio_from_dpu asio_from_dpu.cpp)
target_link_libraries(asio_from_dpu PUBLIC aio Boost::program_options dma_util)
target_include_directories(asio_from_dpu PUBLIC ${PROJECT_SOURCE_DIR}/lib)

add_executable(asio_to_dpu asio_to_dpu.cpp)
target_link_libraries(asio_to_dpu PUBLIC aio Boost::program_options dma_util)
target_include_directories(asio_to_dpu PUBLIC ${PROJECT_SOURCE_DIR}/lib)

#####
add_executable(file_source file_source.cpp)
target_link_libraries(file_source PRIVATE aio Boost::program_options)

add_executable(file_sink file_sink.cpp)
target_link_libraries(file_sink PRIVATE aio Boost::program_options)

