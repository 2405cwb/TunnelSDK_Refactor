win32:CONFIG += win64
QT       += widgets concurrent sql
TARGET    = TunnelViewerSDK
TEMPLATE  = lib
CONFIG   += staticlib  # 🟢 声明为静态库，方便同事直接集成

# 🟢 2. C++ 标准配置
CONFIG += c++11 console
# 如果用的是较新的 Qt6，可能需要 c++17
# CONFIG += c++17
 
 
# 定义头文件搜索路径，方便内部引用
INCLUDEPATH += $$PWD/include \
               $$PWD/include/items \
               $$PWD/include/tools \
               $$PWD/src \
               $$PWD/../ImageSlicer/opencv480/include

win32-msvc*:CONFIG(debug, debug|release) {
    LIBS += -L$$PWD/../ImageSlicer/debug -lopencv_world480d
}

win32-msvc*:CONFIG(release, debug|release) {
    LIBS += -L$$PWD/../ImageSlicer/opencv480/bin -lopencv_world480
}

# 公开给同事的接口头文件
HEADERS += \
    include/AbstractTileSource.h \
    include/tools/AbstractTool.h \
    src/TiledGraphicsView.h \
    src/TunnelSectionItem.h \
	include/items/DefectShapeItem.h \
	src/AsyncImageLoader.h

# 内部实现文件
SOURCES += \
    src/TiledGraphicsView.cpp \
    src/TunnelSectionItem.cpp \
    src/AsyncImageLoader.cpp \
	include/items/DefectShapeItem.cpp
	
win32-msvc* {
    
    # 强制 UTF-8，解决乱码
    QMAKE_CXXFLAGS += /utf-8
}
