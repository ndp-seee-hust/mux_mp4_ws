cmake_minimum_required(VERSION 3.5)

project(codec_sim)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package( Threads REQUIRED )

add_library(codec_ipc_sim STATIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_ipc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc.c
)
target_include_directories(codec_ipc_sim PUBLIC 
    ${PROJECT_SOURCE_DIR}/thirdparty/log
    ${PROJECT_SOURCE_DIR}/thirdparty/h264reader
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(codec_ipc_sim PUBLIC log h264reader pthread)



# add_executable(test_codec_sim sample.c)
# target_include_directories(test_codec_sim PUBLIC 
#     ${PROJECT_SOURCE_DIR}/thirdparty/log
#     ${PROJECT_SOURCE_DIR}
# )
# target_link_libraries(test_codec_sim PUBLIC codec_ipc_sim log)

