project(h264reader)

add_library(h264reader STATIC h264reader.c)
target_include_directories(h264reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

