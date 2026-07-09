# ============================================================
# toolchain_arm32.cmake — ARM32 (armv7l) Linux 交叉编译工具链
#
# 用法:
#   cmake -B build_arm32 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_arm32.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#
# 前提: 需安装 ARM32 交叉编译器
#   sudo apt install g++-arm-linux-gnueabihf
# ============================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器（按实际工具链路径修改）
set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# 目标系统根目录（按实际 sysroot 路径修改）
# set(CMAKE_SYSROOT /path/to/arm32-sysroot)
# set(CMAKE_FIND_ROOT_PATH /path/to/arm32-sysroot)

# 仅在目标 sysroot 中搜索库和头文件
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 强制架构
set(SDK_ARCH arm32 CACHE STRING "Target architecture")
