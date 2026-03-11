cmake_minimum_required(VERSION 3.10)

# 项目名称
project(c_helper C)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 设置源文件列表
set(SOURCE_FILES
    pyhelper.c
    serialqueue.c
    stepcompress.c
    itersolve.c
    trapq.c
    pollreactor.c
    msgblock.c
    trdispatch.c
    kin_cartesian.c
    kin_corexy.c
    kin_corexz.c
    kin_delta.c
    kin_deltesian.c
    kin_polar.c
    kin_rotary_delta.c
    kin_winch.c
    kin_extruder.c
    kin_shaper.c
    kin_idex.c
)

# 设置头文件列表（仅供信息参考，可选）
set(HEADER_FILES
    compiler.h
    list.h
    serialqueue.h
    stepcompress.h
    itersolve.h
    pyhelper.h
    trapq.h
    pollreactor.h
    msgblock.h
    trdispatch.h
    kin_extruder.h
    rpmsg.h
)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -O2 -shared -fPIC -flto -fwhole-program -fno-use-linker-plugin")

# 检测是否支持SSE指令集
include(CheckCCompilerFlag)
check_c_compiler_flag("-mfpmath=sse -msse2" SSE_SUPPORTS)

if(SSE_SUPPORTS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -msse2")
endif()

# 添加目标库
add_library(turbo_core SHARED ${SOURCE_FILES})
