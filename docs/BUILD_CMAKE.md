# laserWeldSDK — CMake 构建指南（跨平台）

## 支持平台

| 平台 | 架构 | 产物 | 状态 |
|------|------|------|------|
| **Windows** | x64 | `laserWeldSDK.dll` | ✅ 已验证 |
| **Windows** | x86 | `laserWeldSDK.dll` | 待验证 |
| **Linux** | x64 | `liblaserWeldSDK.so` | 待验证（cr_sdk 库待部署） |
| **Linux ARM32** | armv7l | `liblaserWeldSDK.so` | 交叉编译支持 |
| **Linux AArch64** | aarch64 | `liblaserWeldSDK.so` | 交叉编译支持 |

> **注意**：源码中无任何平台条件编译（`#ifdef WIN32` 仅用于 DLL 导出宏），所有平台支持在 CMake 构建层面实现。

---

## 环境要求

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| **CMake** | ≥ 3.16 | [cmake.org](https://cmake.org/download/) |
| **MSVC** | VS 2015+ (v140 工具集) | Windows 构建 |
| **GCC/Clang** | 支持 C++17 | Linux 构建 |
| **ARM 交叉编译器** | `arm-linux-gnueabihf-g++` | ARM32 交叉编译 |
| **AArch64 交叉编译器** | `aarch64-linux-gnu-g++` | AArch64 交叉编译 |

> 本项目 `CONFIG -= qt`，构建**不依赖 Qt 库**。

---

## 项目结构

```
git 仓库根目录/
├── CMakeLists.txt                  ← CMake 跨平台构建入口
├── laserWeldSDK.pro                ← qmake 工程文件
│
├── cmake/                          ← CMake 模块
│   ├── platform.cmake              ← 平台检测 + SDK_ARCH 推导
│   ├── toolchain_arm32.cmake       ← ARM32 交叉编译工具链
│   └── toolchain_aarch64.cmake     ← AArch64 交叉编译工具链
│
├── include/laserWeldSDK/           ← 公开 API 头文件
│   ├── cl_api.h
│   ├── cl_basedefine.h
│   ├── cl_craftscript.h
│   ├── clmath.h
│   ├── craftscript.h
│   ├── internaldatadefine.h
│   ├── messageprase.h
│   ├── utils.h
│   ├── brand/brandadapter.h
│   └── common/rs485_communicater.h
│
├── src/                            ← 实现源文件
│   ├── cl_api.cpp
│   ├── clmath.cpp
│   ├── craftscript.cpp
│   ├── messageprase.cpp
│   ├── utils.cpp
│   ├── brand/brandadapter.cpp
│   └── common/rs485_communicater.cpp
│
├── third_party/cr_sdk/             ← CR 机器人 SDK（预编译）
│   ├── include/                    ← 跨平台头文件
│   ├── windows/libs/  (x64, x86)   ← Windows DLL/导入库
│   ├── arm/libs/                   ← ARM32 .so
│   └── aarch64/libs/               ← AArch64 .so
│
├── libs/                           ← 构建产物（按架构分目录）
│   ├── x64/       (laserWeldSDK.dll / liblaserWeldSDK.so)
│   ├── x86/
│   ├── arm32/
│   └── aarch64/
│
└── docs/
    └── BUILD_CMAKE.md
```

---

## 构建步骤

### Windows x64（本机构建）

```cmd
REM 1. 初始化 MSVC 编译环境
call D:\VS\VC\vcvarsall.bat amd64

REM 2. 进入 git 仓库根目录
cd /d C:\Users\wulei\Desktop\gitlab\laserWeldSDK\laserWeldSDK

REM 3. CMake 配置
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release

REM 4. 编译
cmake --build build

REM 5. 验证
dir libs\x64\laserWeldSDK.dll
```

### Linux x64（本机构建）

```bash
cd /path/to/laserWeldSDK
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ls libs/x64/liblaserWeldSDK.so
```

### ARM32 交叉编译

```bash
# 前提：安装 ARM32 交叉编译器
#   sudo apt install g++-arm-linux-gnueabihf

cd /path/to/laserWeldSDK
cmake -B build_arm32 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_arm32.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build_arm32
ls libs/arm32/liblaserWeldSDK.so
```

### AArch64 交叉编译

```bash
# 前提：安装 AArch64 交叉编译器
#   sudo apt install g++-aarch64-linux-gnu

cd /path/to/laserWeldSDK
cmake -B build_aarch64 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_aarch64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build_aarch64
ls libs/aarch64/liblaserWeldSDK.so
```

### 手动指定架构

```bash
# 覆盖自动检测
cmake -B build -DSDK_ARCH=aarch64 -DCMAKE_BUILD_TYPE=Release
```

---

## CMake 配置选项

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release` / `Debug` |
| `SDK_ARCH` | 自动检测 | 手动指定: `x64` / `x86` / `arm32` / `aarch64` |
| `CMAKE_TOOLCHAIN_FILE` | 无 | 交叉编译工具链文件路径 |

---

## 平台架构对应关系

| SDK_ARCH | CMake 检测条件 | cr_sdk 路径 | 产物名 |
|----------|---------------|-----------|--------|
| `x64` | `WIN32` + 64位指针 | `windows/libs/x64/cr_sdk.lib` | `laserWeldSDK.dll` |
| `x86` | `WIN32` + 32位指针 | `windows/libs/x86/cr_sdk.lib` | `laserWeldSDK.dll` |
| `x64` | `Linux` + x86_64 | `linux/libs/x64/libcr_sdk.so` | `liblaserWeldSDK.so` |
| `arm32` | `Linux` + armv7l | `arm/libs/libcr_sdk.so` | `liblaserWeldSDK.so` |
| `aarch64` | `Linux` + aarch64 | `aarch64/libs/libcr_sdk.so` | `liblaserWeldSDK.so` |

---

## 关键编译选项

| CMake 设置 | qmake 等价 | 说明 |
|------------|------------|------|
| `CMAKE_CXX_STANDARD 17` | `CONFIG += c++17` | C++17 标准 |
| `/utf-8` (MSVC) | `CONFIG += utf8_source` | UTF-8 源文件编码 |
| `LASERWELDSDK_LIBRARY` | `DEFINES += LASERWELDSDK_LIBRARY` | 符号导出 |
| `WIN32` / `_WINDOWS` 等 | qmake 自动注入 | Windows 平台宏 |
| `_GNU_SOURCE` | qmake 自动注入 | Linux 平台宏 |

---

## 常见问题

### Q: Windows 编译报错 "常量中有换行符" / C2001

**原因**：源文件 UTF-8 编码与 MSVC 默认 GBK 代码页冲突。

**解决**：CMakeLists.txt 已配置 `/utf-8`。确认 MSVC ≥ 2015 Update 3。

### Q: 交叉编译找不到编译器

```bash
# 安装 ARM 交叉编译器 (Ubuntu/Debian)
sudo apt install g++-arm-linux-gnueabihf    # ARM32
sudo apt install g++-aarch64-linux-gnu      # AArch64
```

如果编译器名称不同，修改 `cmake/toolchain_*.cmake` 中的 `CMAKE_CXX_COMPILER`。

### Q: 链接时找不到 cr_sdk

- Windows: 确认 `third_party/cr_sdk/windows/libs/${SDK_ARCH}/cr_sdk.lib` 存在
- Linux ARM: 确认 `third_party/cr_sdk/arm/libs/libcr_sdk.so` 存在
- Linux AArch64: 确认 `third_party/cr_sdk/aarch64/libs/libcr_sdk.so` 存在
- Linux x64: `third_party/cr_sdk/linux/libs/x64/` 目录尚未创建，如需 Linux x64 构建请先部署 cr_sdk 库

### Q: 运行时找不到 cr_sdk.dll / libcr_sdk.so

部署时将 `third_party/cr_sdk/<平台>/libs/` 下的依赖库复制到可执行文件同级目录，或添加到 `PATH` / `LD_LIBRARY_PATH`。
