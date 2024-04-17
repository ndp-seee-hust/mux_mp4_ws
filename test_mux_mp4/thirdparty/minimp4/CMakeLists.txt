project(minimp4)

add_library(minimp4 STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/minimp4.c)
target_include_directories(minimp4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
