# CMakeList.txt: CatImageDemo 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required(VERSION 3.16)

# 1. 设置 Qt 路径 (请确保路径正确)
set(CMAKE_PREFIX_PATH "C:/Qt/Qt5.8.0/5.8/msvc2015_64") 
# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("CatImageDemo" VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # 强制编译器以 UTF-8 编码读取文件，并以 UTF-8 编码生成可执行文件
    add_compile_options("/utf-8") 
      add_compile_options(/permissive)
    add_compile_options(/Zc:__cplusplus-)
endif()
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Concurrent)
# 将源代码添加到此项目的可执行文件。
add_executable (CatImageDemo   "main.cpp" )


# 链接 Qt 库
target_link_libraries(CatImageDemo PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
     Qt${QT_VERSION_MAJOR}::Concurrent
)

set_target_properties(CatImageDemo PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")

# TODO: 如有需要，请添加测试并安装目标。
