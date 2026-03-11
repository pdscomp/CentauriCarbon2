set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#异常处理库
add_library(exception_handler SHARED
    exception_handler.cpp
)

add_library(utilities SHARED
    utilities.cpp
)

add_library(event_handler SHARED
    event_handler.cpp
)

add_library(any SHARED
    any.cpp
)

# 生成日记库
add_library(elegoo_log SHARED logger.cpp)
target_link_libraries(elegoo_log 
    "${CMAKE_SOURCE_DIR}/lib/spdlog/bin/${COMPILER_PLATFORM}/libspdlog.so"
)
