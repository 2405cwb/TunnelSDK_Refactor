#include <QApplication>

#include "TiledGraphicsView.h"
#include "./MainWindow.h"
int main(int argc, char* argv[])
{
    // 🟢 1. SDK 全局初始化 (必须在 QApplication 之前)
    TiledGraphicsView::init();

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle(QString::fromLocal8Bit("隧道病害检测系统 (SDK完整演示版)"));
    w.show();
    

    return a.exec();

    //测试
}