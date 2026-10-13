# ============================================================
# CMake 编译环境 — laserWeldSDK（跨平台）
#
# 支持平台：
#   Windows        : x64, x86
#   Linux          : x64, arm32, aarch64
#
# 目录结构：
#   include/laserWeldSDK/  — 公开 API 头文件
#   src/                   — 私有实现源文件
#   third_party/cr_sdk/    — CR 机器人 SDK（预编译二进制）
#   cmake/                 — CMake 模块和工具链文件
#   libs/${SDK_ARCH}/      — 构建产物输出
#
# 用法：
#   # 本机构建（自动检测平台）
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
#   # 交叉编译 ARM32
#   cmake -B build_arm32 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_arm32.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#
#   # 交叉编译 AArch64
#   cmake -B build_aarch64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_aarch64.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#
#   # 手动指定架构
#   cmake -B build -DSDK_ARCH=aarch64 -DCMAKE_BUILD_TYPE=Release
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(laserWeldSDK VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# C++ 标准
# ============================================================

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 引入平台检测模块
# ============================================================

include(cmake/platform.cmake)

# ============================================================
# 源文件列表（所有平台共用）
# ============================================================

set(SOURCES
    src/cl_api.cpp
    src/clmath.cpp
    src/craftscript.cpp
    src/messageprase.cpp
    src/utils.cpp
    src/brand/brandadapter.cpp
    src/common/base_commu.cpp
    src/common/rs485_commu.cpp
    src/common/parametercompiler.cpp
    src/common/parameterscript.cpp
)

# ============================================================
# 构建共享库
# ============================================================

add_library(${PROJECT_NAME} SHARED ${SOURCES})

# ============================================================
# Include 路径
# ============================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    include/laserWeldSDK
    include/laserWeldSDK/brand
    include/laserWeldSDK/common
    ${CRSDK_ROOT}/include          # cr_sdk 跨平台头文件（所有平台一致）
)

# ============================================================
# 平台条件编译选项
# ============================================================

# 所有平台共用
target_compile_definitions(${PROJECT_NAME} PRIVATE
    LASERWELDSDK_LIBRARY           # 启用符号导出（Windows: dllexport, Linux: 空宏）
)

# Windows 平台
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32
        _WINDOWS
        _WINDLL
        _UNICODE
        UNICODE
    )
endif()

# Linux 平台
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _GNU_SOURCE
    )
endif()

# UTF-8 源文件编码
#   MSVC: 默认系统代码页(GBK)，需 /utf-8 显式指定
#   GCC/Clang: 默认 UTF-8，无需额外选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ============================================================
# 平台条件可见性设置
# ============================================================

# Windows: __declspec(dllexport) 已由 LASERWELDSDK_EXPORT 宏处理
# Linux:   GCC 默认导出所有符号，若要隐藏需显式设置
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 保持与 .pro 行为一致：导出所有符号
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

# ============================================================
# 链接 cr_sdk（路径和库列表由 cmake/platform.cmake 推导）
# ============================================================

target_link_libraries(${PROJECT_NAME} PRIVATE ${CRSDK_LIBS})

# ============================================================
# 输出目录 — 按平台架构分目录
# ============================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
    ARCHIVE_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
    LIBRARY_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
)

# ============================================================
# 构建信息摘要
# ============================================================

message(STATUS "")
message(STATUS "=== laserWeldSDK Build Config ===")
message(STATUS "  Platform    : ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Architecture: ${SDK_ARCH}")
message(STATUS "  Output      : ${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "=================================")
