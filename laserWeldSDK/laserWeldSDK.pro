CONFIG -= qt

TEMPLATE = lib
DEFINES += LASERWELDSDK_LIBRARY

CONFIG += c++17
# 声明源码使用UTF-8编码
CONFIG += utf8_source

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    cl_api.cpp \
    cl_comandlistscript.cpp \
    cl_craftscript.cpp \
    cl_luascript.cpp \
    cl_utils.cpp

HEADERS += \
    cl_api.h \
    cl_basedefine.h \
    cl_comandlistscript.h \
    cl_craftscript.h \
    cl_internaldatadefine.h \
    cl_luascript.h \
    cr_sdk/include/basestruct.h \
    cr_sdk/include/robotapi.h \
    cl_utils.h

#如果编译时报错找不到WDK相关系统文件，解开这段注释并修改为当前电脑中相应的WDK地址
#INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.17763.0/ucrt"

#win32:CONFIG(release, debug|release): LIBS += -L'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/x64/' -lucrt
#else:win32:CONFIG(debug, debug|release): LIBS += -L'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/x64/' -lucrtd
#else:unix: LIBS += -L'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/x64/' -lucrt

#INCLUDEPATH += 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/x64'
#DEPENDPATH += 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/x64'

win32: LIBS += -L$$PWD/cr_sdk/windows/libs/x64/ -lcr_sdk

INCLUDEPATH += $$PWD/cr_sdk/windows/include
DEPENDPATH += $$PWD/cr_sdk/windows/include

unix:!macx: LIBS += -L$$PWD/cr_sdk/arm/libs/ -lcr_sdk

INCLUDEPATH += $$PWD/cr_sdk/arm/libs
DEPENDPATH += $$PWD/cr_sdk/arm/libs
