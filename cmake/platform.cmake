# ============================================================
# platform.cmake — 跨平台检测与 SDK_ARCH 推导
#
# 自动检测目标平台和架构，推导 SDK_ARCH 变量：
#   Windows: x64 / x86
#   Linux:   x64 / arm32 / aarch64
#
# 也可通过 -DSDK_ARCH=x64 手动覆盖。
# ============================================================

if(NOT DEFINED SDK_ARCH)
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDK_ARCH "x64")
        else()
            set(SDK_ARCH "x86")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
            set(SDK_ARCH "aarch64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm|ARM")
            set(SDK_ARCH "arm32")
        else()
            set(SDK_ARCH "x64")
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

message(STATUS "Target platform : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Target arch     : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "SDK_ARCH        : ${SDK_ARCH}")

# ============================================================
# cr_sdk 库路径选择
# ============================================================

set(CRSDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/cr_sdk")

if(WIN32)
    # Windows: 仅链接 cr_sdk.lib，其他 .dll 为运行时依赖
    if(SDK_ARCH STREQUAL "x64")
        set(CRSDK_LIB_DIR "${CRSDK_ROOT}/windows/libs/x64")
        set(CRSDK_LIBS "${CRSDK_LIB_DIR}/cr_sdk.lib")
    elseif(SDK_ARCH STREQUAL "x86")
        set(CRSDK_LIB_DIR "${CRSDK_ROOT}/windows/libs/x86")
        set(CRSDK_LIBS "${CRSDK_LIB_DIR}/cr_sdk.lib")
    else()
        message(FATAL_ERROR "Unsupported Windows arch: ${SDK_ARCH}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux ARM/AArch64: 需要链接 cr_sdk + 7 个依赖库
    if(SDK_ARCH STREQUAL "arm32")
        set(CRSDK_LIB_DIR "${CRSDK_ROOT}/arm/libs")
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
        set(CRSDK_LIB_DIR "${CRSDK_ROOT}/aarch64/libs")
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
    elseif(SDK_ARCH STREQUAL "x64")
        set(CRSDK_LIB_DIR "${CRSDK_ROOT}/linux/libs/x64")
        set(CRSDK_LIBS "${CRSDK_LIB_DIR}/libcr_sdk.so")
    else()
        message(FATAL_ERROR "Unsupported Linux arch: ${SDK_ARCH}")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "CRSDK_LIB_DIR   : ${CRSDK_LIB_DIR}")
