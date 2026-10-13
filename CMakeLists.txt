# ============================================================
# CMake 编译环境 — laserWeldSDK（跨平台）
#
# 支持 target（host × 工具链）：
#   win-x64        : host=Windows,   Ninja+cl（保留 MSVC2015 兼容）
#   linux-arm32    : host=Ubuntu,    交叉编译 arm-linux-gnueabihf
#   linux-aarch64  : host=Ubuntu,    交叉编译 aarch64-linux-gnu
#
# 目录结构：
#   include/laserWeldSDK/  — 公开 API 头文件
#   src/                   — 私有实现源文件
#   third_party/cr_sdk/    — CR 机器人 SDK（预编译二进制）
#   cmake/                 — CMake 模块和工具链文件
#   libs/${SDK_ARCH}/      — 构建产物输出
#
# 用法（推荐通过 CMakePresets.json）：
#   cmake --preset win-x64       && cmake --build --preset win-x64
#   cmake --preset arm32         && cmake --build --preset arm32
#   cmake --preset aarch64       && cmake --build --preset aarch64
# ============================================================

cmake_minimum_required(VERSION 3.16)

# ============================================================
# 【R8】强制 out-of-source 构建
# ============================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "禁止 in-source 构建！请使用独立构建目录，例如：\n"
        "  cmake --preset win-x64\n"
        "  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_arm32.cmake"
    )
endif()

project(laserWeldSDK VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# C++ 标准 — 保留参数编译链与 MSVC2015 所需的 C++11 兼容性
# ============================================================

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 【R7】MSVC 运行时库：/MD（MultiThreadedDLL），与 cr_sdk DLL 对齐
#   GCC/Linux 无此概念，自动忽略
# ============================================================
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

# ============================================================
# 引入平台检测模块（推导 SDK_ARCH、CRSDK_ROOT、CRSDK_LIBS）
# ============================================================

include(cmake/platform.cmake)

# ============================================================
# 源文件列表（所有平台共用）
#   【修正】rs485_communicater → rs485_commu（匹配实际文件名）
#   【修正】新增 base_commu.cpp（实际存在但基线遗漏）
# ============================================================

set(SOURCES
    src/cl_api.cpp
    src/clmath.cpp
    src/craftscript.cpp
    src/messageprase.cpp
    src/utils.cpp
    src/brand/brandadapter.cpp
    include/laserWeldSDK/brand/lasermodbusrtubuilder.cpp
    src/common/base_commu.cpp
    src/common/rs485_commu.cpp
    src/common/parametercompiler.cpp
    src/common/parameterscript.cpp
    src/brand/deviceparameters.cpp
    src/common/parametercontext.cpp
    src/parameters_api.cpp
)

# ============================================================
# 构建共享库
# ============================================================

add_library(${PROJECT_NAME} SHARED ${SOURCES})

# ============================================================
# 【R3】共享库版本：liblaserWeldSDK.so.1.0.0 → liblaserWeldSDK.so.1 → liblaserWeldSDK.so
# ============================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 1
)

# ============================================================
# 【R2】Include 路径
#   公开头 → PUBLIC（下游 find_package 可访问）
#   私有头 → PRIVATE（仅库内部使用）
# ============================================================

include(GNUInstallDirs)

target_include_directories(${PROJECT_NAME} PUBLIC
    # 构建与安装的头文件路径分别声明，导出包不携带本机源码路径。
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/laserWeldSDK>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/laserWeldSDK>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include/laserWeldSDK/brand        # 品牌适配层（内部实现）
    include/laserWeldSDK/common       # 通讯层（内部实现）
    ${CRSDK_ROOT}/include             # cr_sdk 头文件（第三方依赖）
)

# ============================================================
# 平台条件编译选项
# ============================================================

# 所有平台共用
target_compile_definitions(${PROJECT_NAME} PRIVATE
    LASERWELDSDK_LIBRARY           # 启用符号导出（Windows: dllexport, Linux: 默认可见）
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

# UTF-8 源文件编码（保留基线配置）
#   MSVC: 默认系统代码页(GBK)，需 /utf-8 显式指定
#   GCC/Clang: 默认 UTF-8，无需额外选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ============================================================
# 【R8】--no-undefined：需确认 cr_sdk 所有符号在链接期可见
#   若链接时报 undefined reference 到 cr_sdk 符号，请检查 CRSDK_LIBS 列表是否完整
# ============================================================

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 保持与 .pro 行为一致：导出所有符号
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif()

# ============================================================
# 链接 cr_sdk（路径和库列表由 cmake/platform.cmake 推导）
# ============================================================

target_link_libraries(${PROJECT_NAME} PRIVATE ${CRSDK_LIBS})

# ============================================================
# 输出目录 — 按 SDK_ARCH 分目录
#   win-x64       → libs/x86_64-win/
#   linux-arm32   → libs/arm32/
#   linux-aarch64 → libs/aarch64/
# ============================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
    ARCHIVE_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
    LIBRARY_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}"
)

# ============================================================
# install() 规则 — 供下游 find_package(laserWeldSDK) 使用
# ============================================================

install(TARGETS ${PROJECT_NAME}
    EXPORT  laserWeldSDKTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 公开头文件
install(DIRECTORY include/laserWeldSDK/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laserWeldSDK
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "brand" EXCLUDE
    PATTERN "common" EXCLUDE
)

# 导出 CMake config 包（下游可通过 find_package(laserWeldSDK) 链接）
install(EXPORT laserWeldSDKTargets
    NAMESPACE laserWeldSDK::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/laserWeldSDK
)

# ============================================================
# 构建信息摘要
# ============================================================

message(STATUS "")
message(STATUS "=== laserWeldSDK Build Config ===")
message(STATUS "  Platform    : ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Architecture: ${SDK_ARCH}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  MSVC Runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "  Output      : ${CMAKE_SOURCE_DIR}/libs/${SDK_ARCH}")
message(STATUS "  CRSDK       : ${CRSDK_LIB_DIR}")
message(STATUS "=================================")

# ============================================================
# CTest 冒烟测试 — 验证共享库导出符号存在
#   cmake --build --preset win-x64 && ctest --preset win-x64
# ============================================================

enable_testing()

# 获取构建产物的完整路径
set(SMOKE_LIB_PATH "$<TARGET_FILE:${PROJECT_NAME}>")

if(WIN32)
    # Windows: dumpbin /EXPORTS 检查至少有一个导出符号
    add_test(NAME smoke_exports_win
        COMMAND dumpbin /EXPORTS "${SMOKE_LIB_PATH}"
    )
    set_tests_properties(smoke_exports_win PROPERTIES
        PASS_REGULAR_EXPRESSION "laserWeldSDK"
    )
else()
    # Linux: nm -D 检查动态符号表非空
    add_test(NAME smoke_exports_linux
        COMMAND sh -c "nm -D '${SMOKE_LIB_PATH}' | grep ' T ' | head -5"
    )
    set_tests_properties(smoke_exports_linux PROPERTIES
        PASS_REGULAR_EXPRESSION "T "
    )

    # 验证 SONAME
    add_test(NAME smoke_soname
        COMMAND sh -c "readelf -d '${SMOKE_LIB_PATH}' | grep SONAME"
    )
    set_tests_properties(smoke_soname PROPERTIES
        PASS_REGULAR_EXPRESSION "liblaserWeldSDK\\.so\\.1"
    )
endif()
