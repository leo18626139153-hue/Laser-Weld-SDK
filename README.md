# laserWeldSDK — CMake 跨平台编译指南

## 1. 环境配置

### 1.1 通用要求

| 组件 | 版本 | 说明 |
|------|------|------|
| **CMake** | ≥ 3.16 | [cmake.org](https://cmake.org/download/) |
| **Ninja** | 任意 | Windows 推荐生成器（`choco install ninja` 或下载到 PATH） |
| **Git** | 任意 | 版本控制 |

> **注意**：本项目为纯 C++ 动态库，**不依赖 Qt**。C++ 标准为 **C++17**。

---

### 1.2 Windows（win-x64）

**编译器**：Visual Studio 2017+（MSVC v141+，`cl` ≥ 19.1），**msvc2015 不支持 `/std:c++17`**。

安装 Visual Studio 2017/2019/2022 并勾选"使用 C++ 的桌面开发"工作负载。

```cmd
:: 每次新开终端先初始化 MSVC 环境
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

:: 验证编译器版本
cl /?
```

| 变量 | 值 | 说明 |
|------|-----|------|
| 编译器 | `cl` (MSVC v141+) | 从 vcvarsall.bat 注入 PATH |
| 生成器 | Ninja | `CMAKE_BUILD_TYPE` 可生效 |
| Qt 路径 | 不需要 | 本项目不依赖 Qt |

---

### 1.3 Ubuntu（交叉编译 arm32 / aarch64）

**编译器**：GCC ≥ 9（C++17 完整支持）。

```bash
# 安装交叉编译器
sudo apt install g++-arm-linux-gnueabihf    # ARM32
sudo apt install g++-aarch64-linux-gnu      # AArch64

# 安装 Ninja（可选，也可用 Unix Makefiles）
sudo apt install ninja-build
```

**sysroot 准备**（以 ARM32 为例）：

```bash
# 方案 A：从目标设备 rsync（推荐）
SYSROOT=~/sysroot-arm32
mkdir -p $SYSROOT
rsync -av --copy-links user@<target-ip>:/lib      $SYSROOT/lib
rsync -av --copy-links user@<target-ip>:/usr/lib  $SYSROOT/usr/lib
rsync -av --copy-links user@<target-ip>:/usr/include $SYSROOT/usr/include

# 方案 B：Ubuntu multiarch
sudo apt install libc6-dev-armhf-cross
# sysroot 位于 /usr/arm-linux-gnueabihf/
```

获取 sysroot 后，编辑 `cmake/toolchain_arm32.cmake`，取消注释并填写路径：

```cmake
set(CMAKE_SYSROOT /home/user/sysroot-arm32)
set(CMAKE_FIND_ROOT_PATH /home/user/sysroot-arm32)
```

---

### 1.4 cr_sdk 预编译库校验

CMake 配置阶段会自动检查 `third_party/cr_sdk/` 下四个架构的库是否齐备：

| 架构 | cr_sdk 路径 | 状态要求 |
|------|------------|---------|
| x86_64-win | `third_party/cr_sdk/windows/libs/x64/cr_sdk.lib` | 必须存在 |
| arm32 | `third_party/cr_sdk/arm/libs/libcr_sdk.so` | 必须存在 |
| aarch64 | `third_party/cr_sdk/aarch64/libs/libcr_sdk.so` | 必须存在 |

**缺一即 FATAL_ERROR**，请补全缺失的架构库后重试。

---

## 2. 编译

### 2.1 win-x64（Windows 本机）

```cmd
:: 1. 初始化 MSVC 编译环境（每次新开终端）
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

:: 2. 进入项目目录
cd /d C:\Users\wulei\Desktop\gitlab\laserWeldSDK\laserWeldSDK

:: 3. 配置 + 编译
cmake --preset win-x64
cmake --build --preset win-x64

:: 4. 验证产物
dir libs\x86_64-win\laserWeldSDK.dll
dir libs\x86_64-win\laserWeldSDK.lib
```

> 产物：`libs/x86_64-win/laserWeldSDK.dll` + `laserWeldSDK.lib`（导入库）

---

### 2.2 linux-arm32（Ubuntu 交叉编译）

```bash
# 进入项目目录
cd ~/gitlab/laserWeldSDK/laserWeldSDK

# 配置 + 编译
cmake --preset arm32
cmake --build --preset arm32 -j$(nproc)

# 验证产物
file libs/arm32/liblaserWeldSDK.so
# 应显示: ELF 32-bit LSB shared object, ARM, ...

ls -la libs/arm32/liblaserWeldSDK.so*
# 应有: liblaserWeldSDK.so -> liblaserWeldSDK.so.1
#       liblaserWeldSDK.so.1 -> liblaserWeldSDK.so.1.0.0
#       liblaserWeldSDK.so.1.0.0
```

---

### 2.3 linux-aarch64（Ubuntu 交叉编译）

```bash
cd ~/gitlab/laserWeldSDK/laserWeldSDK

cmake --preset aarch64
cmake --build --preset aarch64 -j$(nproc)

# 验证产物
file libs/aarch64/liblaserWeldSDK.so
# 应显示: ELF 64-bit LSB shared object, ARM aarch64, ...
```

---

### 2.4 不使用 Preset 的手动构建

```bash
# win-x64（本机，Ninja + MSVC）
cmake -B build/win-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDK_ARCH=x86_64-win
cmake --build build/win-x64

# ARM32（交叉编译）
cmake -B build/arm32 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_arm32.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/arm32 -j$(nproc)

# AArch64（交叉编译）
cmake -B build/aarch64 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_aarch64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/aarch64 -j$(nproc)
```

---

## 3. 运行与 DLL 部署

### 3.1 Windows 运行时 DLL 依赖

`laserWeldSDK.dll` 依赖同目录下的 cr_sdk 运行时 DLL：

```
libs/x86_64-win/
├── laserWeldSDK.dll      ← 构建产物
├── laserWeldSDK.lib      ← 导入库（下游链接用）
├── cr_sdk.dll            ← 运行时依赖（从 third_party/cr_sdk/windows/libs/x64/ 复制）
├── protobuf-c.dll
├── protobuf-rpc.dll
├── pthreadVC2.dll
├── zlib.dll
├── CGXIZip.dll
├── LogStorageDll.dll
├── RobotConfigDll.dll
├── tpAlgApp.dll
└── (vcruntime140.dll 等 MSVC 运行时)
```

部署时需将 cr_sdk 所有 `.dll` 复制到 `laserWeldSDK.dll` 同级目录，或加入 `PATH`。

### 3.2 Linux 运行时 .so 部署

目标设备上需将 `liblaserWeldSDK.so*` 和 cr_sdk 所有 `.so` 放入 `/usr/lib/` 或通过 `LD_LIBRARY_PATH` 指定：

```bash
# 方式 A：安装到系统
sudo cp libs/arm32/*.so* /usr/lib/

# 方式 B：LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/libs/arm32:$LD_LIBRARY_PATH
```

---

## 4. 测试

### 4.1 冒烟测试：验证导出符号

```bash
# Windows（在 Developer Command Prompt 中）
dumpbin /EXPORTS libs\x86_64-win\laserWeldSDK.dll

# Linux
nm -D libs/arm32/liblaserWeldSDK.so | grep " T "
readelf -d libs/arm32/liblaserWeldSDK.so | grep SONAME
# 应有: 0x000000000000000e (SONAME) Library soname: [liblaserWeldSDK.so.1]
```

### 4.2 下游链接验证（install 后）

```bash
# 安装到本地前缀
cmake --install build/win-x64 --prefix /tmp/laserWeldSDK_install

# 下游 CMakeLists.txt 示例
# find_package(laserWeldSDK REQUIRED)
# target_link_libraries(my_app PRIVATE laserWeldSDK::laserWeldSDK)
```

---

## 5. 安装

```bash
# 安装到指定前缀（默认 /usr/local）
cmake --install build/win-x64 --prefix ./install

# 安装后的目录结构：
# install/
# ├── bin/laserWeldSDK.dll        (Windows)
# ├── lib/liblaserWeldSDK.so*     (Linux)
# ├── lib/cmake/laserWeldSDK/     ← CMake config 包
# │   └── laserWeldSDKTargets.cmake
# └── include/laserWeldSDK/       ← 公开头文件
#     ├── cl_api.h
#     ├── cl_basedefine.h
#     ├── craftscript.h
#     └── ...
```

---

## 附录 A：VS Code CMake Tools 配置

`.vscode/settings.json`：

```json
{
  "cmake.sourceDirectory": "${workspaceFolder}/laserWeldSDK",
  "cmake.useCMakePresets": "always",
  "cmake.configureOnOpen": false,
  "cmake.buildDirectory": "${workspaceFolder}/laserWeldSDK/build/${buildKit}"
}
```

在 VS Code 底部状态栏选择预设（`win-x64` / `arm32` / `aarch64`），然后 Build。

---

## 附录 B：常见问题

### Q: Windows 报 "常量中有换行符" / C2001

确保 CMakeLists.txt 已配置 `/utf-8`（已包含）。若仍有问题，检查源文件是否为 UTF-8 with BOM。

### Q: 交叉编译找不到编译器

```bash
sudo apt install g++-arm-linux-gnueabihf g++-aarch64-linux-gnu
```

### Q: --no-undefined 报 undefined reference

cr_sdk 的部分符号在链接期不可见。确认 `platform.cmake` 中 `CRSDK_LIBS` 列表与实际 `.so` 文件一致。若仍报错且已确认运行时存在，可临时注释 `--no-undefined`。

### Q: msvc2015 能用吗？

不能。C++17 需要 msvc2017+（VS 2017 15.0+）。msvc2015 (v140) 最高支持 C++14。

### Q: 为什么 Windows 用 Ninja 而不用 VS 生成器？

Ninja 让 `CMAKE_BUILD_TYPE` 直接生效（`Release`/`Debug`）。VS 生成器需要用 `--config Release` 代替。
