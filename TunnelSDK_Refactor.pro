TEMPLATE = subdirs
# 🟢 2. C++ 标准配置
CONFIG += c++11 console
# 如果用的是较新的 Qt6，可能需要 c++17
# CONFIG += c++17
# 定义子项目目录
SUBDIRS += SDK \
           DemoApp

# 🟢 关键：声明依赖关系
# 确保在编译 Demo 之前，SDK 库已经编译好了
DemoApp.depends = SDK
 