#pragma once
#include <QMainWindow>
#include<QObject>
#include <QVBoxLayout>
#include "TunnelViewerController.h" 
#include "SubwaySourceFactory.h"    
#include "AsyncImageLoader.h"
#include <QMenu>          
#include <QMessageBox>    
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug> 
#include "TiledGraphicsView.h"
#include<QSlider>
class MainWindow : public QMainWindow 
{
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private: 
    // 🟢 主入口
    void setupUI();

    // 🟢 拆分后的子功能函数
    void initWindowLayout();      // 初始化窗口和主布局
    void initTopControls();       // 初始化顶部控制栏 (亮度/LOD)
    void initGraphicsView();      // 初始化图形视图 SDK
    void initController();        // 初始化控制器和加载数据
    void initDefectSystem();      // 初始化病害系统
    void initHUD();               // 初始化 HUD (抬头显示层)
    void initConnections();       // 初始化所有信号槽连接
    void exportDefects();
protected:
	void keyPressEvent(QKeyEvent* event) override;
private:
    // 🟢 成员变量 (UI 组件)
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;

    // 顶部控件
    QSlider* m_sliderBrightness;
    QLabel* m_valBrightness;
    QSlider* m_sliderLod;
    QLabel* m_valLod;

    // 核心视图
    TiledGraphicsView* m_view;

    // HUD 控件
    QLabel* m_statsLabel;
    QLabel* m_cursorLabel;

    // 控制器
    TunnelViewerController* m_controller;
};

