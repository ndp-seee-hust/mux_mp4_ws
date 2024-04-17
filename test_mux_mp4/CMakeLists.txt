cmake_minimum_required(VERSION 3.16.3)
project(mp4lib VERSION 1.0)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package( Threads REQUIRED )

# Build fdk-aac lib
add_library(fdk-aac STATIC IMPORTED)
message(${CMAKE_CURRENT_SOURCE_DIR}/libs/libfdk-aac.a)
set_target_properties(fdk-aac PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/libfdk-aac.a")
target_include_directories(fdk-aac INTERFACE
  ${PROJECT_SOURCE_DIR}/thirdparty/libAACdec/include
  ${PROJECT_SOURCE_DIR}/thirdparty/libAACenc/include
  ${PROJECT_SOURCE_DIR}/thirdparty/libFDK/include
)

# Build exc file
add_executable(test_mp4_mux_frame 
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test/test_mp4_mux_frame.c
)
target_include_directories(test_mp4_mux_frame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/minimp4/include)
target_link_libraries(test_mp4_mux_frame PRIVATE minimp4 h264reader log)


add_executable(test_mp4_mux_audio_video 
  ${CMAKE_CURRENT_SOURCE_DIR}/unit_test/test_mux_audio_video.c)
target_include_directories(test_mp4_mux_audio_video PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/minimp4/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/codec_sim
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libAACdec
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libAACenc
    )

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(test_mp4_mux_audio_video PUBLIC "-pthread")
endif()

target_link_libraries(test_mp4_mux_audio_video PRIVATE minimp4 h264reader codec_ipc_sim log fdk-aac)


add_subdirectory(thirdparty)