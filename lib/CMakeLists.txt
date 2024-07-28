add_library(utility
  dma_utils.c
)

target_include_directories(utility
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
