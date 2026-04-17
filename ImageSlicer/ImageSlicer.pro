QT += core gui widgets concurrent

TEMPLATE = app
TARGET = ImageSlicer

# 开启 C++11
CONFIG += c++11 console

# 只有一个源文件
SOURCES += main.cpp

# 处理中文乱码
win32-msvc* {
    QMAKE_CXXFLAGS += /source-charset:utf-8 /execution-charset:utf-8
}