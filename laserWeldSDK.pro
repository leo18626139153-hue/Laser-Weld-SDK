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
# 输出目录
# ============================================================

equals(SDK_ARCH, aarch64) { DESTDIR = $$OUT_ROOT/aarch64 }
equals(SDK_ARCH, arm32)   { DESTDIR = $$OUT_ROOT/arm32 }
equals(SDK_ARCH, x64)     { DESTDIR = $$OUT_ROOT/x64 }
equals(SDK_ARCH, x86)     { DESTDIR = $$OUT_ROOT/x86 }

isEmpty(DESTDIR) {
    error("Unknown SDK_ARCH: $$SDK_ARCH. Please use SDK_ARCH=aarch64/arm32/x64/x86")
}

message("DESTDIR = $$DESTDIR")

# ============================================================
# 中间文件目录
# ============================================================
OBJECTS_DIR = $$BUILD_ROOT/obj/$$SDK_ARCH   #指定 C++ 源文件编译后生成的目标文件（.o 或 .obj）的存放路径。
MOC_DIR     = $$BUILD_ROOT/moc/$$SDK_ARCH   #指定 Qt 的 MOC（元对象编译器） 生成的 moc_*.cpp 文件的存放路径。
RCC_DIR     = $$BUILD_ROOT/rcc/$$SDK_ARCH   #指定 Qt 的 RCC（资源编译器） 生成的 qrc_*.cpp 文件的存放路径。
UI_DIR      = $$BUILD_ROOT/ui/$$SDK_ARCH    #指定 Qt 的 UIC（UI 编译器） 生成的 ui_*.h 头文件的存放路径。

# ============================================================
# 源.文.件
# ============================================================ 
SOURCES += \
    src/brand/brandadapter.cpp \
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
        CRSDK_LIB_DIR = $$CRSDK_ROOT/windows/libs/x64
        exists($$CRSDK_LIB_DIR/cr_sdk.lib) {
            LIBS += -L$$CRSDK_LIB_DIR -lcr_sdk
            DEPENDPATH += $$CRSDK_LIB_DIR
            INCLUDEPATH += $$CRSDK_ROOT/windows/include
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
            INCLUDEPATH += $$CRSDK_ROOT/windows/include
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
        CRSDK_LIB_DIR = $$CRSDK_ROOT/arm/libs
        CRSDK_INC_DIR = $$CRSDK_ROOT/arm/include
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
        CRSDK_LIB_DIR = $$CRSDK_ROOT/aarch64/libs
        CRSDK_INC_DIR = $$CRSDK_ROOT/aarch64/include
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
