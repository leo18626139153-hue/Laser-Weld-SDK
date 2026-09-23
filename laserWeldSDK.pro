# ============================================================
# laserWeldSDK — qmake 工程文件（跨平台）
#
# 支持平台：
#   Windows        : x64, x86
#   Linux          : x64, arm32, aarch64
#
# 用法：
#   qmake laserWeldSDK.pro "SDK_ARCH=x64" CONFIG+=release
#   qmake laserWeldSDK.pro "SDK_ARCH=aarch64" CONFIG+=release
#
# 产物路径（debug / release 已隔离，互不覆盖）：
#   Windows x64 : libs/x64/release/laserWeldSDK.dll  与  libs/x64/debug/laserWeldSDK.dll
#   Linux/ARM   : libs/<aarch64|arm32|x64>/<release|debug>/liblaserWeldSDK.so*
#   中间文件    : build/obj/<arch>/<release|debug>/
# ============================================================

CONFIG -= qt

TARGET = laserWeldSDK
TEMPLATE = lib

DEFINES += LASERWELDSDK_LIBRARY
DEFINES += QT_DEPRECATED_WARNINGS

CONFIG += c++14
CONFIG += utf8_source

# ============================================================
# 路径说明（重构后）
#
# 当前工程结构：
# git 仓库根目录/             ← $$PWD (CMakeLists.txt / .pro 所在)
# ├── include/laserWeldSDK/   ← 公开 API 头文件
# ├── src/                    ← 私有实现源文件
# ├── third_party/cr_sdk/     ← CR 机器人 SDK
# └── libs/                   ← 构建产物输出
# ============================================================

SRC_ROOT     = $$PWD
OUT_ROOT     = $$PWD/libs
BUILD_ROOT   = $$PWD/build
CRSDK_ROOT   = $$PWD/third_party/cr_sdk

message("========== laserWeldSDK path ==========")
message("SRC_ROOT     = $$SRC_ROOT")
message("OUT_ROOT     = $$OUT_ROOT")
message("BUILD_ROOT   = $$BUILD_ROOT")
message("CRSDK_ROOT   = $$CRSDK_ROOT")
message("=======================================")

# ============================================================
# 架构选择
# ============================================================
isEmpty(SDK_ARCH) {
    win32 {
        contains(QT_ARCH, i386) {
            SDK_ARCH = x86
        } else {
            SDK_ARCH = x64
        }
    }

    unix:!macx {
        contains(QMAKE_CXX, aarch64)|contains(QMAKE_CC, aarch64)|contains(QT_ARCH, aarch64)|contains(QT_ARCH, arm64) {
            SDK_ARCH = aarch64
        } else:contains(QMAKE_CXX, arm)|contains(QMAKE_CC, arm)|contains(QT_ARCH, arm) {
            SDK_ARCH = arm32
        } else {
            SDK_ARCH = x64
        }
    }
}

message("========== laserWeldSDK build ==========")
message("SDK_ARCH = $$SDK_ARCH")
message("========================================")

# ============================================================
# 构建配置（debug / release）区分
# ============================================================
# 【修改理由】原工程 Debug 与 Release 共用同一个 OBJECTS_DIR（build/obj/<arch>）
#   和同一个 DESTDIR（libs/<arch>），由此产生两个问题：
#   1) 两个配置的同名 .obj 互相复用：nmake 依据文件时间戳判定目标"已是最新"而跳过编译，
#      "Debug 构建"实际链接的是之前 Release 编译出来的 .obj，最终 DLL 是 -O2 优化过的
#      Release 代码（依赖 MSVCP140.dll/VCRUNTIME140.dll 而非 D 版），
#      调试时 PDB 与源码行号对不上，断点无法命中；
#   2) 两个配置的 DLL/PDB 互相覆盖，无法判断 libs/<arch> 里当前是哪一种产物。
#
# 【逻辑梳理】用 CONFIG(debug, debug|release) 判定当前构建配置（qmake 生成
#   Makefile.Debug / Makefile.Release 时会分别求值本文件，因此两者取值不同），
#   把中间产物与输出产物分别落到 debug/、release/ 子目录，使两个配置完全隔离、
#   可同时存在：debug 版为 /Zi + /MDd（可调试），release 版为 -O2 + /MD（发布用）。
#   注意：BUILD_FLAVOR 必须在下面所有 DESTDIR / OBJECTS_DIR 赋值之前定义。
CONFIG(debug, debug|release) {
    BUILD_FLAVOR = debug
} else {
    BUILD_FLAVOR = release
}

message("BUILD_FLAVOR = $$BUILD_FLAVOR")

# ============================================================
# 输出目录（按架构 + 构建配置区分）
# ============================================================

equals(SDK_ARCH, aarch64) { DESTDIR = $$OUT_ROOT/aarch64/$$BUILD_FLAVOR }
equals(SDK_ARCH, arm32)   { DESTDIR = $$OUT_ROOT/arm32/$$BUILD_FLAVOR }
equals(SDK_ARCH, x64)     { DESTDIR = $$OUT_ROOT/x64/$$BUILD_FLAVOR }
equals(SDK_ARCH, x86)     { DESTDIR = $$OUT_ROOT/x86/$$BUILD_FLAVOR }

isEmpty(DESTDIR) {
    error("Unknown SDK_ARCH: $$SDK_ARCH. Please use SDK_ARCH=aarch64/arm32/x64/x86")
}

message("DESTDIR = $$DESTDIR")

# ============================================================
# 中间文件目录（按架构 + 构建配置区分，避免 debug/release 的目标文件互相复用）
# ============================================================
OBJECTS_DIR = $$BUILD_ROOT/obj/$$SDK_ARCH/$$BUILD_FLAVOR   #指定 C++ 源文件编译后生成的目标文件（.o 或 .obj）的存放路径。
MOC_DIR     = $$BUILD_ROOT/moc/$$SDK_ARCH/$$BUILD_FLAVOR   #指定 Qt 的 MOC（元对象编译器） 生成的 moc_*.cpp 文件的存放路径。
RCC_DIR     = $$BUILD_ROOT/rcc/$$SDK_ARCH/$$BUILD_FLAVOR   #指定 Qt 的 RCC（资源编译器） 生成的 qrc_*.cpp 文件的存放路径。
UI_DIR      = $$BUILD_ROOT/ui/$$SDK_ARCH/$$BUILD_FLAVOR    #指定 Qt 的 UIC（UI 编译器） 生成的 ui_*.h 头文件的存放路径。

# ============================================================
# 源.文.件
# ============================================================ 
SOURCES += \
    src/brand/brandadapter.cpp \
    include/laserWeldSDK/brand/lasermodbusrtubuilder.cpp \
    src/cl_api.cpp \
    src/clmath.cpp \
    src/common/base_commu.cpp \
    src/common/rs485_commu.cpp \
    src/craftscript.cpp \
    src/messageprase.cpp \
    src/utils.cpp
# 头.文.件
HEADERS += \
    include/laserWeldSDK/brand/brandadapter.h \
    include/laserWeldSDK/cl_api.h \
    include/laserWeldSDK/cl_basedefine.h \
    include/laserWeldSDK/clmath.h \
    include/laserWeldSDK/common/base_commu.h \
    include/laserWeldSDK/common/rs485_commu.h \
    include/laserWeldSDK/craftscript.h \
    include/laserWeldSDK/internaldatadefine.h \
    include/laserWeldSDK/messageprase.h \
    include/laserWeldSDK/utils.h

# cr_sdk 头文件（不参与编译，仅用于 IDE 索引）
HEADERS += \
    $$CRSDK_ROOT/include/basestruct.h \
    $$CRSDK_ROOT/include/robotapi.h

# ============================================================
# 公共 include
# ============================================================
INCLUDEPATH += $$SRC_ROOT/include/laserWeldSDK
INCLUDEPATH += $$SRC_ROOT/include/laserWeldSDK/brand
INCLUDEPATH += $$SRC_ROOT/include/laserWeldSDK/common
INCLUDEPATH += $$CRSDK_ROOT/include

# ============================================================
# Windows / MSVC
# ============================================================
win32 {
    message("-- platform: Windows")
    equals(SDK_ARCH, x64) {
        message("-- link cr_sdk: windows x64")
        CRSDK_LIB_DIR = $$CRSDK_ROOT/libs/x64
        exists($$CRSDK_LIB_DIR/cr_sdk.lib) {
            LIBS += -L$$CRSDK_LIB_DIR -lcr_sdk
            DEPENDPATH += $$CRSDK_LIB_DIR
            INCLUDEPATH += $$CRSDK_ROOT/include
        } else {
            error("Can not find Windows x64 cr_sdk.lib: $$CRSDK_LIB_DIR")
        }
    }

    equals(SDK_ARCH, x86) {
        message("-- link cr_sdk: windows x86")
        CRSDK_LIB_DIR = $$CRSDK_ROOT/windows/libs/x86
        exists($$CRSDK_LIB_DIR/cr_sdk.lib) {
            LIBS += -L$$CRSDK_LIB_DIR -lcr_sdk
            DEPENDPATH += $$CRSDK_LIB_DIR
            INCLUDEPATH += $$CRSDK_ROOT/include
        } else {
            error("Can not find Windows x86 cr_sdk.lib: $$CRSDK_LIB_DIR")
        }
    }
}

# ============================================================
# Linux / ARM32 / AArch64 / x64
# ============================================================
unix:!macx {
    message("-- platform: Linux")

    equals(SDK_ARCH, arm32) {
        message("-- link cr_sdk: linux arm32")
        CRSDK_LIB_DIR = $$CRSDK_ROOT/libs/arm
        CRSDK_INC_DIR = $$CRSDK_ROOT/include
        exists($$CRSDK_LIB_DIR/libcr_sdk.so) {
            INCLUDEPATH += $$CRSDK_INC_DIR
            DEPENDPATH += $$CRSDK_LIB_DIR
            LIBS += -L$$CRSDK_LIB_DIR \
                    -lcr_sdk \
                    -lprotobuf-c \
                    -lprotobuf-rpc \
                    -lLogStorageDll \
                    -lRobotConfigDll \
                    -lCGXIZip \
                    -ltpAlgApp \
                    -lz
        } else {
            error("Can not find Linux arm32 libcr_sdk.so: $$CRSDK_LIB_DIR")
        }
    }

    equals(SDK_ARCH, aarch64) {
        message("-- link cr_sdk: linux aarch64")
        CRSDK_LIB_DIR = $$CRSDK_ROOT/libs/AARCH64-Release
        CRSDK_INC_DIR = $$CRSDK_ROOT/include
        exists($$CRSDK_LIB_DIR/libcr_sdk.so) {
            INCLUDEPATH += $$CRSDK_INC_DIR
            DEPENDPATH += $$CRSDK_LIB_DIR
            LIBS += -L$$CRSDK_LIB_DIR \
                    -lcr_sdk \
                    -lprotobuf-c \
                    -lprotobuf-rpc \
                    -lLogStorageDll \
                    -lRobotConfigDll \
                    -lCGXIZip \
                    -ltpAlgApp \
                    -lz
        } else {
            error("Can not find Linux aarch64 libcr_sdk.so: $$CRSDK_LIB_DIR")
        }
    }

    equals(SDK_ARCH, x64) {
        message("-- link cr_sdk: linux x64")
        CRSDK_LIB_DIR = $$CRSDK_ROOT/linux/libs/x64
        CRSDK_INC_DIR = $$CRSDK_ROOT/linux/include
        exists($$CRSDK_LIB_DIR/libcr_sdk.so) {
            INCLUDEPATH += $$CRSDK_INC_DIR
            DEPENDPATH += $$CRSDK_LIB_DIR
            LIBS += -L$$CRSDK_LIB_DIR -lcr_sdk
        } else {
            error("Can not find Linux x64 libcr_sdk.so. Expected path: $$CRSDK_LIB_DIR")
        }
    }
}
