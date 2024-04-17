project(log)

add_library(log STATIC log.c)
target_include_directories(log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
