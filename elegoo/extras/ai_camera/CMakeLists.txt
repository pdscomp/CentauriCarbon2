cmake_minimum_required(VERSION 3.10)

project(ai_camera)

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.1)
        add_compile_options(-Wno-psabi)
    endif()
endif()

# 检测编译器平台
if(CMAKE_C_COMPILER MATCHES "arm")
    set(COMPILER_PLATFORM "arm")
elseif(CMAKE_C_COMPILER MATCHES "gcc")
    set(COMPILER_PLATFORM "x86_64")
else()
    set(COMPILER_PLATFORM "x86_64")
    message(WARNING "The toolchain information is not specified, using the system default configuration.")
endif()

# 设置第三方库目录
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/../../lib)

# 添加头文件目录
set(MODULE_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module")
set(LOGS_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/logs")
set(COMMON_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/common")
set(BASE_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/base")
set(EXCEPT_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/execption")
set(CAMERA_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/camera")
set(UART_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/uart")
set(COMM_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/comm")
set(VIDEO_STAREAM_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/video_stream")
set(UTIL_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/module/util")

set(SPDLOG_INCLUDE_PATH "${THIRD_PARTY_DIR}/spdlog/include")
# set(RTSPSRC_NET_INCLUDE_PATH "${THIRD_PARTY_DIR}/RtspSrc/inc/net")
# set(RTSPSRC_XOP_INCLUDE_PATH "${THIRD_PARTY_DIR}/RtspSrc/inc/xop")
set(JSON_INCLUDE_PATH "${THIRD_PARTY_DIR}/nlohmann")
set(LIBHV_INCLUDE_PATH "${THIRD_PARTY_DIR}/libhv/include")
set(FFMPEG_INCLUDE_PATH "${THIRD_PARTY_DIR}/ffmpeg/include")
set(LIBJPEG_INCLUDE_PATH "${THIRD_PARTY_DIR}/libjpeg/include")
set(LIBAGORA_INCLUDE_PATH "${THIRD_PARTY_DIR}/agora_sdk/include")

# 添加包含目录
include_directories(
    ${MODULE_INCLUDE_PATH}
    ${LOGS_INCLUDE_PATH}
    ${COMMON_INCLUDE_PATH}
    ${BASE_INCLUDE_PATH}
    ${EXCEPT_INCLUDE_PATH}
    ${CAMERA_INCLUDE_PATH}
    ${UART_INCLUDE_PATH}
    ${COMM_INCLUDE_PATH}
    ${VIDEO_STAREAM_INCLUDE_PATH}
    ${UTIL_INCLUDE_PATH}
    ${SPDLOG_INCLUDE_PATH}
    # ${RTSPSRC_NET_INCLUDE_PATH}
    # ${RTSPSRC_XOP_INCLUDE_PATH}
    ${JSON_INCLUDE_PATH}
    ${LIBHV_INCLUDE_PATH}
    ${FFMPEG_INCLUDE_PATH}
    ${LIBJPEG_INCLUDE_PATH}
    ${LIBAGORA_INCLUDE_PATH}
)

# 收集源文件
# file(GLOB_RECURSE SOURCES
#     "${THIRD_PARTY_DIR}/RtspSrc/src/net/*.cpp"
#     "${THIRD_PARTY_DIR}/RtspSrc/src/xop/*.cpp"
# )

# 显式列出特定源文件
set(SOURCE_FILES
    ${SOURCES}
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${PROJECT_SOURCE_DIR}/module/control.cpp
    ${PROJECT_SOURCE_DIR}/module/logs/logs.cpp
    ${PROJECT_SOURCE_DIR}/module/camera/ai_camera.cpp
    ${PROJECT_SOURCE_DIR}/module/camera/uvc_camera_without_uart.cpp
    # ${PROJECT_SOURCE_DIR}/module/camera/uvc_camera.cpp
    ${PROJECT_SOURCE_DIR}/module/camera/uvc_v4l2.cpp
    ${PROJECT_SOURCE_DIR}/module/camera/video_list.cpp
    ${PROJECT_SOURCE_DIR}/module/comm/server_socket.cpp
    ${PROJECT_SOURCE_DIR}/module/comm/comm_protocol.cpp
    ${PROJECT_SOURCE_DIR}/module/video_stream/video_streamer.cpp
    ${PROJECT_SOURCE_DIR}/module/video_stream/rtc_streamer.cpp
    # ${PROJECT_SOURCE_DIR}/module/uart/uart.cpp
    ${PROJECT_SOURCE_DIR}/module/util/ai_camera_pthread.cpp
    ${PROJECT_SOURCE_DIR}/module/util/common_tools.cpp
    ${PROJECT_SOURCE_DIR}/module/util/obico_client.cpp
    ${PROJECT_SOURCE_DIR}/module/util/crc16.c
    ${PROJECT_SOURCE_DIR}/module/util/store_time.cpp
    ${PROJECT_SOURCE_DIR}/module/util/video_info.cpp
    ${PROJECT_SOURCE_DIR}/module/util/video_format_convert.cpp
    ${PROJECT_SOURCE_DIR}/module/util/time_lapse_control.cpp
    ${PROJECT_SOURCE_DIR}/module/util/video_encoder.cpp
)

# 创建可执行文件
add_executable(ai_camera ${SOURCE_FILES})

# 链接第三方库和系统库
target_link_libraries(ai_camera
    # "${THIRD_PARTY_DIR}/libzmq/lib/${COMPILER_PLATFORM}/libzmq.so"
    "${THIRD_PARTY_DIR}/libhv/lib/${COMPILER_PLATFORM}/libhv.so"
    "${THIRD_PARTY_DIR}/agora_sdk/lib/${COMPILER_PLATFORM}/libagora-rtc-sdk.so"
    "${THIRD_PARTY_DIR}/curl/bin/${COMPILER_PLATFORM}/libcurl.so"
    "${THIRD_PARTY_DIR}/openssl/bin/${COMPILER_PLATFORM}/libssl.so"
    "${THIRD_PARTY_DIR}/openssl/bin/${COMPILER_PLATFORM}/libcrypto.so"
    "${THIRD_PARTY_DIR}/libjpeg/lib/libjpeg.so"
    "${THIRD_PARTY_DIR}/ffmpeg/lib/libavcodec.so"
    "${THIRD_PARTY_DIR}/ffmpeg/lib/libavformat.so"
    "${THIRD_PARTY_DIR}/ffmpeg/lib/libavutil.so"
    "${THIRD_PARTY_DIR}/ffmpeg/lib/libavfilter.so"
    "${THIRD_PARTY_DIR}/ffmpeg/lib/libswscale.so"
    pthread
    stdc++
)


add_executable(client_socket  ${PROJECT_SOURCE_DIR}/test/client_socket.cpp)

add_executable(rtm_example  ${PROJECT_SOURCE_DIR}/test/rtm_example.cpp ${PROJECT_SOURCE_DIR}/test/rtm_communicator.cpp)

target_link_libraries(rtm_example
    "${THIRD_PARTY_DIR}/agora_sdk/lib/${COMPILER_PLATFORM}/libagora-rtc-sdk.so"
    pthread
    stdc++
)