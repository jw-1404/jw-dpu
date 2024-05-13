add_executable(jw_to_device dma_to_device.c)
target_link_libraries(jw_to_device PUBLIC dma_util)
target_include_directories(jw_to_device PUBLIC
${PROJECT_SOURCE_DIR}/lib)

add_executable(jw_from_device dma_from_device.c)
target_link_libraries(jw_from_device PUBLIC dma_util)
target_include_directories(jw_from_device PUBLIC
  ${PROJECT_SOURCE_DIR}/lib)

add_executable(reg_rw reg_rw.c)
add_executable(test_chrdev test_chrdev.c)
