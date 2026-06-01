# 🚇 Tunnel Defect Annotation System

![Qt Version](https://img.shields.io/badge/Qt-5.8%2B-green.svg) ![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg) ![License](https://img.shields.io/badge/License-MIT-orange.svg)

这是一个基于 **Qt 5.8 (C++)** 开发的高性能大图浏览及标注系统。它专为处理**超高分辨率**（GB级别）的隧道全景扫描图而设计，采用 **LOD (Level of Detail)** 和 **动态切片加载** 技术，实现了在普通 PC 上流畅浏览、缩放和标注巨大的隧道影像。

项目包含两个独立的部分：

1.  **ImageSlicer**: 数据预处理工具，负责将原始大图切片。
2.  **TunnelSystem**: 主程序，负责渲染、交互与标注。

---

## ✨ 核心特性 (Key Features)

* **🚀 极致性能**:
    * 采用 Google Maps 式的瓦片加载技术，只加载视野内的切片。
    * **LOD 机制**: 缩小时显示极速缩略图，放大时异步加载高清切片。
    * **多线程架构**: 图片读取与解码在后台线程完成，主界面永不卡顿。
* **🛠️ 专业标注**:
    * 支持折线（Line）与闭合多边形（Polygon）绘制。
    * 自动吸附与橡皮筋效果预览。
    * 支持标注裂缝、渗水、剥落等多种病害类型。
    * **二分查找定位**: 毫秒级定位病害所属的图片图幅，并保存为 JSON 数据。
* **📊 实时 HUD**:
    * 显示显存占用、切片数量、渲染耗时、鼠标实时坐标。
    * 右上角包含动态操作指引。
* **🖱️ 交互友好**:
    * 支持 WASD 键盘漫游、鼠标拖拽、滚轮缩放。
    * 右键菜单管理标注（删除/编辑）。

---

## 💻 开发环境 (Environment)

本项目对编译器和位数有严格要求，请务必遵守：

* **IDE**: Visual Studio 2015 / 2017 / 2019 / 2022 (配合 Qt VS Tools) 或 Qt Creator。
* **Qt 版本**: Qt 5.8.0 或更高版本 (推荐使用 Qt 5.15 LTS 以获得更好体验)。
* **构建套件 (Kit)**: **必须使用 MSVC 64-bit**。
  * ⚠️ **警告**: 请勿使用 32-bit 编译器，处理大图时会因内存不足直接崩溃。
* **字符编码**: 源码文件需保存为 **UTF-8 (with signature/BOM)** 或在 `.pro` 中指定 MSVC 编码参数（项目已配置）。

---

## 📂 项目结构

```text
TunnelSystem/
├── main.cpp                # 主程序入口
├── mainwindow.cpp          # 主窗口逻辑
├── TunnelSystem.pro        # 主程序 Qt 工程文件
├── core/
│   └── asyncimageloader.cpp # 异步图片加载器 (单例 + 线程池)
├── items/
│   ├── tunnelsectionitem.cpp # 隧道图元 (负责瓦片调度与LOD)
│   └── defectitems.cpp       # 病害图元 (绘制逻辑)
└── ImageSlicer/            # [独立工具] 切图程序
    ├── main.cpp            # 切图工具入口
    └── ImageSlicer.pro     # 切图工具 Qt 工程文件
```
## 🚀 快速开始 (Quick Start)
第一步：准备数据 (使用 ImageSlicer)
主程序不能直接读取几十 GB 的原始图片，需先切片。

打开 ImageSlicer/ImageSlicer.pro 并编译运行（强烈建议使用 Release 64bit 模式，速度快 10 倍）。

按照弹窗提示：

选择 1: 包含原始大图（.jpg/.png）的文件夹。

选择 2: 切片结果输出的文件夹。

等待控制台显示“全部完成”。

数据结构说明: 切片完成后，每个图片会生成一个文件夹，包含 info.json (元数据)、thumbnail.jpg (缩略图) 和 x_y.jpg (瓦片)。

第二步：运行主程序
使用 Qt Creator 或 VS 打开根目录下的 TunnelSystem.pro。

选择 Debug 或 Release 模式 (必须是 64-bit)。

运行程序。

在弹出的对话框中，选择上一步生成的切片输出目录。

## 操作指南 (User Manual)
⌨️ 快捷键
| 按键 | 功能 |
| :---: | :---: |
| F1 | 切换到 浏览模式 (View Mode) |
|F2|切换到 绘图模式 (Draw Mode)|
|WASD / ↑↓←→|移动视野 (按住 Shift 加速)|
|Ctrl + 滚轮|以鼠标为中心缩放视图|
|ESC|取消当前正在绘制的操作|

🖱️鼠标操作
* 浏览模式:
  * 左键拖拽：平移地图。
  * 右键点击病害：弹出菜单（删除病害）。 

* 绘图模式:
    * 左键点击：添加节点。
    * 移动鼠标：预览线条路径。
    * 右键点击：结束绘制。系统会询问是否闭合（是=多边形，否=折线），并录入病害名称。

## 常见问题 (Troubleshooting)
* Q1: 编译报错 "Unexpected UTF-8 BOM"？
  * 原因: qmake 老版本对带 BOM 的 .pro 文件解析有问题。
  * 解决: 用 Notepad++ 打开 .pro 文件，选择“编码” -> “转为 UTF-8 无 BOM 编码格式”并保存。或者在 VS 中选择“高级保存选项” -> “UTF-8 无签名”。
* Q2: 运行报错 ASSERT: "!weakref.load()"？
  * 原因: 这是旧版 Qt 智能指针析构的已知问题。
  * 解决: 请执行 清理 (Clean) -> 重新生成 (Rebuild)。代码中已使用 QPointer 替代 QSharedPointer 进行安全守卫，必须重新编译才能生效。
* Q3: 切图工具提示 "out of memory" 或崩溃？
  * 原因: 你可能使用了 32-bit 编译器，最大只能申请 2GB 内存。
  * 解决: 切换到 MSVC 64-bit 构建套件。
* Q4: 程序提示 "Unable to set geometry..."？
  * 原因: 申请的窗口尺寸超过了屏幕分辨率（常见于高分屏 150% 缩放）。
  * 解决: 代码已默认使用 showMaximized()，该警告可忽略。

📜 许可证
本项目采用 MIT 许可证。详见 LICENSE 文件。 