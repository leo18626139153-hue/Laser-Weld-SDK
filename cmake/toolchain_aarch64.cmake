# ============================================================
# toolchain_aarch64.cmake — AArch64 (arm64) Linux 交叉编译工具链
#
# host: Ubuntu（推荐；Windows 用户请通过 WSL 使用）
# target: aarch64-linux-gnu
#
# 用法:
#   cmake --preset aarch64
#   cmake -B build_aarch64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_aarch64.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#
# 前提: 安装 AArch64 交叉编译器
#   sudo apt install g++-aarch64-linux-gnu
#
# sysroot 获取（二选一）：
#   a) 从目标设备拷贝：
#      rsync -av --copy-links <user>@<device>:/lib      <sysroot>/lib
#      rsync -av --copy-links <user>@<device>:/usr/lib  <sysroot>/usr/lib
#      rsync -av --copy-links <user>@<device>:/usr/include <sysroot>/usr/include
#   b) 使用 Linaro 预编译 sysroot 或 Ubuntu multiarch
# ============================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 交叉编译器（按实际安装调整名称）
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 目标系统根目录 — 按实际路径修改
# set(CMAKE_SYSROOT /path/to/aarch64-sysroot)
# set(CMAKE_FIND_ROOT_PATH /path/to/aarch64-sysroot)

# 仅在目标 sysroot 中搜索库和头文件，不在 host 搜索
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 强制架构（覆盖自动推导）
set(SDK_ARCH aarch64 CACHE STRING "Target architecture")
