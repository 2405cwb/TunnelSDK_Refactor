win32:CONFIG += win64
QT       += widgets

TEMPLATE = app
TARGET   = DemoApp

# 🟢 2. C++ 标准配置
CONFIG += c++11 console
# 如果用的是较新的 Qt6，可能需要 c++17
# CONFIG += c++17
# 自动定位生成的 .lib 路径


CONFIG(debug, debug|release) {
    LIBS += -L$$OUT_PWD/../SDK/debug/ -lTunnelViewerSDK
} else {
    LIBS += -L$$PWD/../bin/Release-X64/ -lTunnelViewerSDK
}

# 1. 包含 SDK 的头文件路径，这样同事写代码才有自动补全
INCLUDEPATH += $$PWD/../SDK/include \
               $$PWD/../SDK/src \
			   $$PWD/../SDK/src/items \
               $$PWD/../ImageSlicer/opencv480/include


SOURCES += main.cpp 
HEADERS += SubwayTileSource.h

win32-msvc* {
    
    # 强制 UTF-8，解决乱码
    QMAKE_CXXFLAGS += /utf-8
}
