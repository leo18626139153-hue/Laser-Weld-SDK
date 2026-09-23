# ============================================================
# platform.cmake — 跨平台检测与 SDK_ARCH 推导
#
# SDK_ARCH 取值（按 target 分）：
#   win-x64        → x86_64-win
#   linux-arm32    → arm32
#   linux-aarch64  → aarch64
#
# 推导规则：
#   交叉编译时取 CMAKE_SYSTEM_PROCESSOR（目标架构），非 host 架构
#   本机构建时自动检测
#   也可通过 -DSDK_ARCH=xxx 手动覆盖
# ============================================================

# ============================================================
# 1. 推导 SDK_ARCH（交叉编译优先取 target，否则取 host）
# ============================================================

if(NOT DEFINED SDK_ARCH)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # Windows 仅支持 x64 本机构建
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDK_ARCH "x86_64-win")
        else()
            message(FATAL_ERROR "win-x86 不在当前支持列表中，请使用 x64 工具链")
        endif()

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # CMAKE_SYSTEM_PROCESSOR 在交叉编译时反映目标架构
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
            set(SDK_ARCH "aarch64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm|ARM")
            set(SDK_ARCH "arm32")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            message(FATAL_ERROR
                "linux-x64 不在当前支持列表中。\n"
                "支持的 target: arm32, aarch64, win-x64"
            )
        else()
            message(FATAL_ERROR "无法识别的 Linux 目标架构: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()

    else()
        message(FATAL_ERROR "不支持的操作系统: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

message(STATUS "Target system   : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "SDK_ARCH        : ${SDK_ARCH}")

# ============================================================
# 2. 【Q3】cr_sdk 四架构库齐备检查
#    缺一即 FATAL_ERROR，不得静默继续
# ============================================================

set(CRSDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/cr_sdk")

# 定义每个 SDK_ARCH 对应的 cr_sdk 库目录（相对于 CRSDK_ROOT）
set(CRSDK_DIR_x86_64-win  "${CRSDK_ROOT}/libs/x64")
set(CRSDK_DIR_arm32       "${CRSDK_ROOT}/libs/arm")
set(CRSDK_DIR_aarch64     "${CRSDK_ROOT}/libs/AARCH64-Release")

# 头文件目录（所有架构共用）
set(CRSDK_INCLUDE_DIR "${CRSDK_ROOT}/include")

# 检查头文件
if(NOT EXISTS "${CRSDK_INCLUDE_DIR}/robotapi.h")
    message(FATAL_ERROR "cr_sdk 头文件缺失: ${CRSDK_INCLUDE_DIR}/robotapi.h")
endif()

# 逐架构校验库文件
foreach(_arch x86_64-win arm32 aarch64)
    if(DEFINED CRSDK_DIR_${_arch})
        if(NOT EXISTS "${CRSDK_DIR_${_arch}}")
            message(FATAL_ERROR
                "cr_sdk 库目录缺失: ${CRSDK_DIR_${_arch}}\n"
                "架构 ${_arch} 的预编译库不完整，请补充 third_party/cr_sdk/ 下的库文件"
            )
        endif()
        message(STATUS "cr_sdk ${_arch}  : ${CRSDK_DIR_${_arch}} [OK]")
    endif()
endforeach()

# ============================================================
# 3. 按当前 SDK_ARCH 选择 cr_sdk 库列表
# ============================================================

if(SDK_ARCH STREQUAL "x86_64-win")
    set(CRSDK_LIB_DIR "${CRSDK_DIR_x86_64-win}")
    # Windows: 链接 cr_sdk.lib（导入库），运行时依赖同目录下的 .dll
    set(CRSDK_LIBS "${CRSDK_LIB_DIR}/cr_sdk.lib")

elseif(SDK_ARCH STREQUAL "arm32")
    set(CRSDK_LIB_DIR "${CRSDK_DIR_arm32}")
    # ARM32 Linux: 需要 cr_sdk + 7 个依赖 .so
    set(CRSDK_LIBS
        "${CRSDK_LIB_DIR}/libcr_sdk.so"
        "${CRSDK_LIB_DIR}/libprotobuf-c.so"
        "${CRSDK_LIB_DIR}/libprotobuf-rpc.so"
        "${CRSDK_LIB_DIR}/libLogStorageDll.so"
        "${CRSDK_LIB_DIR}/libRobotConfigDll.so"
        "${CRSDK_LIB_DIR}/libCGXIZip.so"
        "${CRSDK_LIB_DIR}/libtpAlgApp.so"
        "${CRSDK_LIB_DIR}/libz.so"
    )

elseif(SDK_ARCH STREQUAL "aarch64")
    set(CRSDK_LIB_DIR "${CRSDK_DIR_aarch64}")
    set(CRSDK_LIBS
        "${CRSDK_LIB_DIR}/libcr_sdk.so"
        "${CRSDK_LIB_DIR}/libprotobuf-c.so"
        "${CRSDK_LIB_DIR}/libprotobuf-rpc.so"
        "${CRSDK_LIB_DIR}/libLogStorageDll.so"
        "${CRSDK_LIB_DIR}/libRobotConfigDll.so"
        "${CRSDK_LIB_DIR}/libCGXIZip.so"
        "${CRSDK_LIB_DIR}/libtpAlgApp.so"
        "${CRSDK_LIB_DIR}/libz.so"
    )

else()
    message(FATAL_ERROR "不支持的 SDK_ARCH: ${SDK_ARCH}")
endif()

message(STATUS "CRSDK_LIB_DIR   : ${CRSDK_LIB_DIR}")
